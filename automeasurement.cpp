#include "automeasurement.h"
#include "databuffer.h"
#include "datameasurement.h"
#include "typemeasurement.h"
#include "settingsmanager.h"
#include "calculatemesurement.h"

#include <algorithm>
#include <QtGlobal>
#include <QTimer>
#include <cmath>

// ===== ctor =====
AutoMeasurement::AutoMeasurement(DataBuffer* buffer,
                                 DataMeasurement* storage,
                                 QObject* parent)
    : QObject(parent), m_buffer(buffer), m_storage(storage)
{
    Q_ASSERT(m_buffer && "AutoMeasurement: buffer must not be null");
    Q_ASSERT(m_storage && "AutoMeasurement: storage must not be null");
    m_local.reserve(AUTOMEAS_BUFFER_SIZE);                         // предвыделение
}

// ===== createPlan (только калькулятор, без побочных эффектов) =====
AutoSavePlan AutoMeasurement::createPlan(const StepSettings& step_setting,
                                         const AutoSaveSettings& auto_setting,
                                         const DataMeasurement* data_mesurement) const
{
    AutoSavePlan plan;                                              // создаём пустой план

    // --- переносим пороги из auto_setting
    plan.cfg.positiveTolerance = auto_setting.positiveTolerance;
    plan.cfg.negativeTolerance = auto_setting.negativeTolerance;
    plan.cfg.speedLimit        = auto_setting.speedLimit;
    plan.cfg.speedWindow       = 30;
    plan.cfg.speedStride       = 5;
    plan.cfg.exitHysteresis    = 1.5;
    plan.cfg.stableTicks       = 15;
    plan.cfg.cooldownTicks     = 15;
    plan.cfg.exitSpeedMul      = 2.0;
    plan.cfg.exitDistance      = 0.005;

    // --- исходная точка: просто "последний шаг и сколько на нём уже измерений"
    int lastStep   = 1;
        int measOnLast = 0;
        if (data_mesurement && !data_mesurement->groups().isEmpty()) {
            const auto& groups = data_mesurement->groups();
            const auto& lastGroup = groups.last();
            if (!lastGroup.steps.isEmpty()) {
                const auto& lastSeries = lastGroup.steps.last();
                lastStep   = std::max(1, lastSeries.stepNumber);
                measOnLast = std::max(0, (int)lastSeries.measurements.size());
            }
     }

    // --- полезные локальные
    const int N = std::max(1, step_setting.count);                 // всего шагов
    const int repeatsTotal = std::max(1, step_setting.repeatCount);// повторов на шаг

    auto expectedFor = [&](int s)->double {
        return CalculateMesurement::expected(step_setting.mode, s, step_setting.step,
                                             step_setting.manualText, step_setting.formula, step_setting.formulaCount);
    };

    // --- добавляем текущий шаг (остаток повторов = repeatsTotal - measOnLast, но не меньше 1 цикла)
    {
        SaveZone z;
        z.stepNumber   = lastStep;
        z.expected     = expectedFor(z.stepNumber);
        z.repeatsTotal = std::max(0, repeatsTotal - measOnLast);
        if (z.repeatsTotal == 0) z.repeatsTotal = repeatsTotal;     // если добито — начнём полный цикл
        plan.zones.push_back(z);
    }

    // --- если autoGroup=false → только текущий шаг
    if (!auto_setting.autoGroup)
        return plan;

    // --- иначе достраиваем серию вперёд до N
    for (int s = lastStep + 1; s <= N; ++s) {
        SaveZone z;
        z.stepNumber   = s;
        z.expected     = expectedFor(s);
        z.repeatsTotal = repeatsTotal;
        plan.zones.push_back(z);
    }

    // --- если шаги bidi — дополняем дублем N и обратным ходом (по твоей общей логике)
    if (step_setting.bidirectional) {
        SaveZone dupN;
        dupN.stepNumber   = N;
        dupN.expected     = expectedFor(N);
        dupN.repeatsTotal = repeatsTotal;
        plan.zones.push_back(dupN);
        for (int s = N - 1; s >= 1; --s) {
            SaveZone z;
            z.stepNumber   = s;
            z.expected     = expectedFor(s);
            z.repeatsTotal = repeatsTotal;
            plan.zones.push_back(z);
        }
    }

    return plan;                                                    // план готов
}

// ===== start (копируем план, подписываемся на буфер, запускаем FSM) =====
void AutoMeasurement::start(const AutoSavePlan& plan)
{
    // --- сохраняем план
    m_plan = plan;

    // --- сбрасываем позицию/счётчики/буфер
    m_zoneIndex  = (m_plan.zones.isEmpty() ? -1 : 0);
    m_doneInZone = 0;
    m_local.clear();
    m_local.squeeze(); // держим компактно
    m_local.reserve(AUTOMEAS_BUFFER_SIZE);
    m_lastSavedDistance = std::numeric_limits<double>::quiet_NaN();
    m_stableTicksAcc = 0;
    m_cooldownLeft   = 0;
    m_saveRequested  = false;

    // --- подключаемся к DataBuffer только здесь
    if (m_bufConn) {
        QObject::disconnect(m_bufConn);
        m_bufConn = QMetaObject::Connection();
    }
    if (m_zoneIndex >= 0) {
        m_bufConn = connect(m_buffer, &DataBuffer::updated,
                            this,     &AutoMeasurement::onBufferUpdated,
                            Qt::UniqueConnection);
    }

    // --- устанавливаем состояние Save (по требованию) и сразу переходим в поиск попадания
    m_state = State::InZoneSearch;                                         // как просил — начнём с Save
    OnState(State::InZoneSearch);                                  // и тут же ищем зону
}

// ===== stop (полный сброс и отписка от буфера) =====
void AutoMeasurement::stop()
{
    if (m_bufConn) {
        QObject::disconnect(m_bufConn);
        m_bufConn = QMetaObject::Connection();
    }
    m_state = State::Idle;
    m_zoneIndex  = -1;
    m_doneInZone = 0;
    m_saveRequested = false;
    m_local.clear();
    m_lastSavedDistance = std::numeric_limits<double>::quiet_NaN();
    m_prevGroups = m_prevSteps = m_prevMeas = 0;
    m_stableTicksAcc = 0;
    m_cooldownLeft = 0;
    m_plan = AutoSavePlan{};
}

// ===== onBufferUpdated (сохраняем новые значения в локальный циклический буфер) =====
void AutoMeasurement::onBufferUpdated(const QVector<double>& values)
{
    if (m_state == State::Idle) return;                             // если не запущены — игнор
    if (values.isEmpty()) return;
    const double x = values.back();                                  // берём последний сэмпл
    m_local.push_back(x);                                            // кладём в локальный буфер
    if (m_local.size() > AUTOMEAS_BUFFER_SIZE) m_local.remove(0);    // делаем буфер цикличным

    // --- параллельность "по факту": OnState сам себя перепланирует через QTimer
}

// ===== savingFinished (зовёт MainWindow, мы проверяем коммит и двигаем указатель плана) =====
void AutoMeasurement::savingFinished()
{
    if (m_state != State::Save) return;                              // актуально только в Save

    if (wasNewSaveCommitted()) {                                     // проверяем, что запись произошла
        PlanPointer();                                               // двигаем указатель плана
        m_cooldownLeft = m_plan.cfg.cooldownTicks;                   // антидребезг
        m_stableTicksAcc = 0;                                        // на всякий случай, чтобы новый заход потребовал реальной “тиши”
        m_saveRequested = false;                                     // готово, можем снова триггерить
        // --- выбираем следующее состояние
        m_state = (m_zoneIndex >= 0 && m_zoneIndex < m_plan.zones.size())
                  ? State::OutZoneSearch : State::Finish;
    }
    // --- если записи не было — остаёмся в Save (ждём повторного вызова savingFinished)
    scheduleNext(m_state);                                           // продолжаем цикл
}

// ===== OnState (ядро FSM, вызывается снова с задержкой через scheduleNext) =====
void AutoMeasurement::OnState(State s)
{
    if (m_zoneIndex < 0 || m_zoneIndex >= m_plan.zones.size()) {
        m_state = State::Finish;                                     // нет зон → финиш
    }

    // --- готовим необходимые величины
    const double distance = (m_local.isEmpty() ? std::numeric_limits<double>::quiet_NaN()
                                               : m_local.back());
    const double expected = (m_zoneIndex>=0 && m_zoneIndex<m_plan.zones.size()
                             ? m_plan.zones[m_zoneIndex].expected
                             : std::numeric_limits<double>::quiet_NaN());

    switch (s) {
    case State::InZoneSearch: {
        // --- сначала даём кулдауну дойти до нуля
        if (m_cooldownLeft > 0) { --m_cooldownLeft; scheduleNext(State::InZoneSearch); break; }

        // --- если и зона, и скорость ок → переходим в Save
        const bool zoneOk = isZoneCorrect(distance, expected);
        bool speedOk = false;

        // Проверку скорости делаем только внутри зоны
        if (zoneOk) {
            speedOk = isSpeedCorrect();
        }
        if (zoneOk && speedOk) {
            m_state = State::Save;
            scheduleNext(State::Save);                               // перейдём к триггеру сейва
        } else {
            // --- иначе продолжаем искать (без рекурсии, через singleShot)
            scheduleNext(State::InZoneSearch);
        }
        break;
    }

    case State::Save: {
        if (!m_saveRequested) {
            // --- запомним "до" для верификации и отправим запрос на сейв
            const auto& groups = m_storage->groups();
            m_prevGroups = groups.size();
            m_prevSteps  = (m_prevGroups > 0 ? groups.last().steps.size() : 0);
            m_prevMeas   = (m_prevGroups > 0 && m_prevSteps > 0 ? groups.last().steps.last().measurements.size(): 0);
            m_lastSavedDistance = distance;                          // точка сейва (для выхода в None)
            m_saveRequested = true;
            emit requestSaving();                                    // MainWindow начнёт сохранение
        }
        // --- ждём savingFinished() от MainWindow
        scheduleNext(State::Save);
        break;
    }

    case State::OutZoneSearch: {
        // --- ждём реального выезда из зоны (гистерезис/ускорение/сдвиг)
        const bool outOk = isOutZoneCorrect(distance, expected);
        if (outOk) {
            m_state = State::InZoneSearch;                           // можно искать следующую зону
            m_stableTicksAcc = 0;                                    // заново копим “тихие тики” для нового захода
            scheduleNext(State::InZoneSearch);
        } else {
            scheduleNext(State::OutZoneSearch);
        }
        break;
    }

    case State::Finish: {
        emit planFinished();                                         // сообщаем MainWindow
        stop();                                                      // план выполнен
        break;
    }

    case State::Idle:
    default:
        break;
    }
}

// ===== мини-утилиты: зона, скорость, выход =====
bool AutoMeasurement::isZoneCorrect(double distance, double expected) const
{
    if (std::isnan(distance)) return false;                          // нет данных → ещё рано
    if (std::isnan(expected)) return true;                           // None: "зона" = "тихо"
    const double dev = distance - expected;
    return (dev >= -m_plan.cfg.negativeTolerance) &&
           (dev <=  m_plan.cfg.positiveTolerance);
}

bool AutoMeasurement::isSpeedCorrect()
{
    const double spd = robustSpeed();                                // робастная "скорость"
    const bool slow  = (spd <= m_plan.cfg.speedLimit);
    accumulateStability(slow);                                       // копим стабильность
    return (m_stableTicksAcc >= m_plan.cfg.stableTicks);             // выдержали длительность
}

bool AutoMeasurement::isOutZoneCorrect(double distance, double expected) const
{
    if (std::isnan(distance)) return false;
    if (!std::isnan(expected)) {
        const double dev = std::fabs(distance - expected);
        const double maxTol = std::max(std::fabs(m_plan.cfg.positiveTolerance),
                                       std::fabs(m_plan.cfg.negativeTolerance));
        return dev > maxTol * m_plan.cfg.exitHysteresis;             // вышли по гистерезису
    } else {
        // --- режим None: "ускорение" или "сдвиг" от точки сейва
        const double spd = robustSpeed();
        const bool fast = (spd >= m_plan.cfg.speedLimit * m_plan.cfg.exitSpeedMul);
        const bool moved = (!std::isnan(m_lastSavedDistance) &&
                            std::fabs(distance - m_lastSavedDistance) >= m_plan.cfg.exitDistance);
        return fast || moved;
    }
}

// ===== скорость/стабильность =====
double AutoMeasurement::robustSpeed() const
{
    const int n = m_local.size();
    const int K = std::min(m_plan.cfg.speedStride, std::max(1, n - 1));
    const int W = std::min(m_plan.cfg.speedWindow, n - K);
    if (W <= 0) return std::numeric_limits<double>::infinity();

    QVector<double> deltas;
    deltas.reserve(W);

    // берём |x[i+K] - x[i]| на последних W отрезках
    for (int i = n - W - 1; i >= 0 && (int)deltas.size() < W; --i) {
        const int j = i + K;
        if (j < n) deltas.push_back(std::fabs(m_local[j] - m_local[i]));
    }
    if (deltas.isEmpty()) return std::numeric_limits<double>::infinity();

    std::sort(deltas.begin(), deltas.end());
    const int mid = deltas.size() / 2;
    return (deltas.size() % 2)
         ? deltas[mid]
         : 0.5 * (deltas[mid - 1] + deltas[mid]);
}


void AutoMeasurement::accumulateStability(bool inside_and_slow)
{
    if (inside_and_slow) ++m_stableTicksAcc;
    else m_stableTicksAcc = 0;
}

// ===== служебное =====
void AutoMeasurement::scheduleNext(State s, int ms)
{
    QTimer::singleShot(ms, this, [this, s](){
        // --- не мешаем стопу: если остановлено, выходим
        if (m_state == State::Idle) return;
        if (s != m_state) return;
        OnState(s);
    });
}

bool AutoMeasurement::wasNewSaveCommitted() const
{
    const auto& groupsRef = m_storage->groups();
    const int groups = groupsRef.size();
    const int steps  = (groups > 0 ? groupsRef.last().steps.size() : 0);
    const int meas   = (groups > 0 && steps > 0
                        ? groupsRef.last().steps.last().measurements.size()
                        : 0);

    // --- сравниваем со "снимком" до сейва
    if (groups > m_prevGroups) return true;
    if (groups == m_prevGroups && steps > m_prevSteps) return true;
    if (groups == m_prevGroups && steps == m_prevSteps && meas > m_prevMeas) return true;
    return false;
}

void AutoMeasurement::PlanPointer()
{
    if (m_zoneIndex < 0 || m_zoneIndex >= m_plan.zones.size()) return;
    const SaveZone& Z = m_plan.zones[m_zoneIndex];

    // --- закрыли один "слот" в зоне
    ++m_doneInZone;

    // --- если зона исчерпана — переходим к следующей
    if (m_doneInZone >= Z.repeatsTotal) {
        ++m_zoneIndex;
        m_doneInZone = 0;
        // --- если план кончился — Finish
        if (m_zoneIndex >= m_plan.zones.size()) {
            m_state = State::Finish;
        }
    }
}



// AutoMeasurement.h
#ifndef AUTOMEASUREMENT_H
#define AUTOMEASUREMENT_H

#include <QObject>
#include <QVector>
#include <limits>

// ----- параметры буфера и тиков
#define AUTOMEAS_BUFFER_SIZE 100         // храним последние N значений
#define AUTOMEAS_STATE_POLL_MS 10        // задержка между повторами OnState

// ----- вперёд-объявления
class DataBuffer;                        // источник онлайн-данных
class DataMeasurement;                   // склад (read-only проверки)
struct StepSettings;                     // настройки шагов
struct AutoSaveSettings;                 // настройки автосохранений

// ----- зона плана (обычно = один шаг)
struct SaveZone {
    int    stepNumber = 1;                                       // номер шага
    double expected   = std::numeric_limits<double>::quiet_NaN();// целевое значение (NaN для None)
    int    repeatsTotal = 1;                                     // сколько раз сохранить в шаге
};

// ----- конфиг рантайма плана (самодостаточные пороги)
struct AutoPlanConfig {
    double positiveTolerance = 0.0;                              // +допуск
    double negativeTolerance = 0.0;                              // −допуск
    double speedLimit        = 0.0;                              // лимит "скорости"
    int    speedWindow       = 12;                               // окно для скорости (W)
    int    speedStride       = 3;                                // шаг для скорости (K)
    double exitHysteresis    = 1.5;                              // гистерезис выхода из зоны
    int    stableTicks       = 8;                                // тиков подряд должно быть "тихо"
    int    cooldownTicks     = 8;                                // антидребезг после сейва
    double exitSpeedMul      = 2.0;                              // множитель порога для выхода (None)
    double exitDistance      = 0.005;                            // мин. сдвиг для выхода (None)
};

// ----- сам план (самодостаточный контракт)
struct AutoSavePlan {
    QVector<SaveZone> zones;                                     // последовательность шагов/зон
    AutoPlanConfig    cfg;                                       // пороги рантайма
};

// ----- исполнитель плана
class AutoMeasurement : public QObject
{
    Q_OBJECT
public:
    explicit AutoMeasurement(DataBuffer* buffer,
                             DataMeasurement* storage,
                             QObject* parent = nullptr);

    // 1) создать план (калькулятор, ничего в поля класса не пишет)
    AutoSavePlan createPlan(const StepSettings& step_setting,
                            const AutoSaveSettings& auto_setting,
                            const DataMeasurement* data_mesurement) const;

    // 2) старт исполнения по выбранному плану
    void start(const AutoSavePlan& plan);

    // 3) полный стоп и очистка
    void stop();

    // 4) Публичный метод-град
    bool isRunning() const { return m_state != State::Idle; }

signals:
    void requestSaving();                                        // просим MainWindow начать сохранение
    void planFinished();                                         // план окончен — сообщаем оркестратору

public slots:
    void savingFinished();                                       // зовёт MainWindow по завершении сейва

private slots:
    void onBufferUpdated(const QVector<double>& values);         // тики из DataBuffer

private:
    // ----- FSM состояния
    enum class State { InZoneSearch, Save, OutZoneSearch, Finish, Idle };

    // ----- основной обработчик состояния
    void OnState(State s);                                       // детерминированный шаг автомата

    // ----- мини-утилиты
    bool isZoneCorrect(double distance, double expected) const;  // expected±tolerance
    bool isSpeedCorrect();                                       // скорость ≤ лимит и стабильность
    bool isOutZoneCorrect(double distance, double expected) const; // вышли за гистерезис/критерии None

    // ----- помощь для скорости/стабильности
    double robustSpeed() const;                                   // медиана |x[i]-x[i-K]| на окне
    void   accumulateStability(bool inside_and_slow);             // накапливаем "тихие тики"

    // ----- служебное
    void   scheduleNext(State s, int ms = AUTOMEAS_STATE_POLL_MS);// повторный вызов OnState с задержкой
    bool   wasNewSaveCommitted() const;                           // проверка, что DataMeasurement пополнился
    void   PlanPointer();                                         // переход к следующему пункту плана

private:
    // зависимости
    DataBuffer*       m_buffer   = nullptr;                       // буфер с онлайн-данными
    DataMeasurement*  m_storage  = nullptr;                       // склад для проверок
    QMetaObject::Connection m_bufConn;                            // коннект на время исполнения

    // фиксированный план (после start)
    AutoSavePlan      m_plan{};                                   // используемый план

    // позиция в плане
    int               m_zoneIndex  = -1;                          // индекс текущей зоны
    int               m_doneInZone = 0;                           // сколько сейвов сделано в зоне

    // FSM
    State             m_state      = State::Idle;                 // текущее состояние
    bool              m_saveRequested = false;                    // чтобы не дублировать requestSaving()

    // локальный циклический буфер дистанций
    QVector<double>   m_local;                                     // последние значения
    double            m_lastSavedDistance = std::numeric_limits<double>::quiet_NaN(); // для None

    // метрики для проверки коммита в DataMeasurement
    int               m_prevGroups = 0;                           // счётчики "до сейва"
    int               m_prevSteps  = 0;
    int               m_prevMeas   = 0;

    // стабильность
    int               m_stableTicksAcc = 0;                       // копим "тихие тики"
    int               m_cooldownLeft   = 0;                       // антидребезг после сейва
};

#endif // AUTOMEASUREMENT_H

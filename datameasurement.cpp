#include "datameasurement.h"
#include "calculatemesurement.h"
#include <algorithm>
#include <cmath>

// ------------------------
// Публичные методы
// ------------------------
void DataMeasurement::applyPlanSave(QSharedPointer<const PlanSave> plan)
{
    m_plan = std::move(plan);
    m_cursor = 0;
}

void DataMeasurement::add(double value)
{
    if (!m_plan || m_plan->items.isEmpty())
        return;

    // 1) Детекция изменений (0/1/2)
    const int change = detectChange();

    if (change == 2) {
        // Новая структура: первый проход с начала
        m_firstPass = true;
        m_cursor = 0;
    } else if (change == 1) {
        // Изменилась база: пересчитать только текущую группу
        if (hasGroup()) {
            recalcCurrentGroupByBaseAndPlan();
        }
    }
    // Зафиксировать актуальные значения для последующих сравнений
    m_prevStructureHash = m_plan->baseStructureHash;
    m_prevBase          = m_plan->base;

    // 2) Снимок текущего элемента плана с учётом NONE/firstPass
    takeSnapshot(value);

    // 3) Исполнить действие
    switch (m_snap.action) {
    case SaveAction::NewGroup:
        startNewGroup();
        break;
    case SaveAction::NewStep:
        startNewStep();
        break;
    case SaveAction::Measurement:
        addNewMeasurement();
        break;
    }

    // 4) Сместить курсор (и при wrap снять firstPass)
    advanceCursor();
}

void DataMeasurement::clear()
{
    m_groups.clear();
    m_groupIdCounter = 0;

    // Сброс курсора/первого прохода/разрешений NONE
    m_cursor = 0;
    m_firstPass = true;

    // Сделаем так, чтобы следующий applyPlanSave() воспринимался как «новый план»
    m_prevStructureHash = 0; // намеренно «другой»
    m_prevBase = std::numeric_limits<double>::quiet_NaN();
}

void DataMeasurement::setGroups(const QVector<MeasurementGroup>& groups)
{
    m_groups = groups;

    // Обновим счётчик groupId
    int maxId = 0;
    for (const auto& g : m_groups) maxId = std::max(maxId, g.groupId);
    m_groupIdCounter = maxId;
}

// ------------------------
// Приватные: детектор изменений
// ------------------------
int DataMeasurement::detectChange() const
{
    if (!m_plan) return 0;

    const bool structureChanged = (m_plan->baseStructureHash != m_prevStructureHash);
    if (structureChanged) return 2;

    const bool baseChanged = !(std::isnan(m_plan->base) && std::isnan(m_prevBase))
                             && (m_plan->base != m_prevBase);
    if (baseChanged) return 1;

    return 0;
}

// ------------------------
// Приватные: снимок элемента плана
// ------------------------
void DataMeasurement::takeSnapshot(double value)
{
    const auto& it = m_plan->items[m_cursor];

    // 1) действие: первый проход или последующие
    m_snap.action = m_firstPass ? it.actionFirst : it.action;

    // 2) базовые поля (как в плане)
    m_snap.stepNumber = it.stepNumber;
    m_snap.expected   = it.expected;
    m_snap.address    = it.address;
    m_snap.direction  = it.direction;
    m_snap.value      = value;

    // 3) режим NONE: все решения делаем здесь (НИКАКИХ промежуточных полей)
    const bool isNone = (it.stepNumber == PlanGenConst::kStepNone);

     // При NewGroup — просто оставляем «как в плане», всё остальное сделают start*()
    if (m_snap.action == SaveAction::NewGroup) {
        return;
    }

    if (isNone) {
        if (m_snap.action == SaveAction::NewStep) {
            // Вычислить новый номер шага и адрес для новой серии
            int nextStep = hasGroup() ? maxStepNumberInCurrentGroup() + 1 : 1;
            if (nextStep <= 0) nextStep = 1;

            int nextAddr = hasGroup() ? maxAddressInCurrentGroup() + 1 : 0;

            m_snap.stepNumber = nextStep;
            m_snap.address    = nextAddr;
            m_snap.expected   = std::numeric_limits<double>::quiet_NaN();
            m_snap.direction  = ApproachDirection::Unknown;
        } else {
            // Measurement в NONE на первом проходе (r > 0):
            // пишем в УЖЕ открытый текущий шаг/серию
            if (hasGroup() && !currentGroup().steps.isEmpty()) {
                m_snap.stepNumber = currentSeries().stepNumber;
                // адрес для хранения не используется — достаточно шаг/серия по факту
            } else {
                // страховка: если вдруг нет серии, считаем как NewStep
                int nextStep = hasGroup() ? maxStepNumberInCurrentGroup() + 1 : 1;
                if (nextStep <= 0) nextStep = 1;
                m_snap.stepNumber = nextStep;
            }
            m_snap.expected   = std::numeric_limits<double>::quiet_NaN();
            m_snap.direction  = ApproachDirection::Unknown;
        }
    }
}

// ------------------------
// Приватные: движение курсора
// ------------------------
void DataMeasurement::advanceCursor()
{
    if (!m_plan || m_plan->items.isEmpty()) return;

    m_cursor += 1;
    if (m_cursor >= m_plan->items.size()) {
        m_cursor = 0;
        // закончили первый цикл — снимаем флаг
        if (m_firstPass) m_firstPass = false;
    }
}

// ------------------------
// Приватные: исполнение действий
// ------------------------
void DataMeasurement::startNewGroup()
{
    // Открыть новую группу
    MeasurementGroup g;
    g.groupId  = ++m_groupIdCounter;
    g.biditype = m_plan ? m_plan->bidirectional : true;

    m_groups.push_back(g);

    startNewStep();
}

void DataMeasurement::startNewStep()
{

    MeasurementSeries s;
    s.stepNumber = m_snap.stepNumber;
    s.expected   = m_snap.expected;
    s.direction  = m_snap.direction;

    currentGroup().steps.push_back(std::move(s));

     addNewMeasurement();
}

void DataMeasurement::addNewMeasurement()
{
    MeasurementSeries& series = currentGroup().steps.last();

    Measurement m;
    m.repeatIndex = series.measurements.size() + 1;
    m.raw         = m_snap.value;

    // distance/deviation считаем от базы из плана и ожидаемого на уровне серии
    const double dist = CalculateMesurement::distance(m_snap.value, m_plan ? m_plan->base : 0.0);
    const double dev  = CalculateMesurement::deviation(dist, m_snap.expected);

    m.distance    = dist;
    m.deviation   = dev;

    series.measurements.push_back(std::move(m));
}

// ------------------------
// Приватные: пересчёт по смене базы (только текущая группа)
// ------------------------
void DataMeasurement::recalcCurrentGroupByBaseAndPlan()
{
    if (!m_plan || !hasGroup()) return;

    MeasurementGroup& g = currentGroup();

    // Быстрый доступ к expected/direction из плана по номеру шага (для не-NONE)
    auto findInPlan = [&](int stepNumber)->std::pair<double, ApproachDirection> {
        for (const auto& it : m_plan->items) {
            if (it.stepNumber == stepNumber) {
                return { it.expected, it.direction };
            }
        }
        // если в плане нет (например, для NONE авто-нумерации) — ожидаемое NaN, направление Unknown
        return { std::numeric_limits<double>::quiet_NaN(), ApproachDirection::Unknown };
    };

    for (auto& s : g.steps) {
        // expected/direction берём из плана (если есть совпадение по stepNumber)
        const auto [expFromPlan, dirFromPlan] = findInPlan(s.stepNumber);
        s.expected  = expFromPlan;
        s.direction = dirFromPlan;

        for (auto& m : s.measurements) {
            m.distance  = CalculateMesurement::distance(m.raw, m_plan->base);
            m.deviation = CalculateMesurement::deviation(m.distance, s.expected);
        }
    }
}

// ------------------------
// Приватные: хелперы для NONE
// ------------------------
int DataMeasurement::maxStepNumberInCurrentGroup() const
{
    if (!hasGroup() || currentGroup().steps.isEmpty())
        return 0;
    int m = 0;
    for (const auto& s : currentGroup().steps) m = std::max(m, s.stepNumber);
    return m;
}

int DataMeasurement::maxAddressInCurrentGroup() const
{
    if (!hasGroup() || currentGroup().steps.isEmpty())
        return -1; // чтобы +1 дал 0 для первой серии
    // Адрес на уровне склада мы не храним отдельно, используем индекс шага как адрес.
    return currentGroup().steps.size() - 1;
}

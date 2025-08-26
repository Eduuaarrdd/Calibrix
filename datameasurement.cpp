#include "datameasurement.h"            // заголовок класса
#include "calculatemesurement.h"        // независимый калькулятор
#include <algorithm>                    // std::max
#include <cmath>                        // isnan

// публичная проверка изменений структуры настроек
bool DataMeasurement::isStepStructureChanged(const StepSettings& a,
                                             const StepSettings& b) const
{
    // режим задания шагов
    if (a.mode != b.mode) return true;                      // сменился режим
    // равномерный шаг
    if (a.step != b.step) return true;                      // сменился шаг
    // число шагов
    if (a.count != b.count) return true;                    // сменилось число шагов
    // Manual текст
    if (a.manualText != b.manualText) return true;          // сменился текст ручного режима
    // формула
    if (a.formula != b.formula) return true;                // сменилось выражение формулы
    // параметр формулы
    if (a.formulaCount != b.formulaCount) return true;      // сменилось число для формулы
    // двунаправленность
    if (a.bidirectional != b.bidirectional) return true;    // сменился тип направления
    // повторы шага структуру не меняют
    return false;                                           // изменений структуры нет
}

// поставить новые настройки (с учётом пересчёта при смене базы)
void DataMeasurement::setStepSettings(const StepSettings& settings)
{
    // вычисляем флаги изменений
    m_stepStructureChanged = isStepStructureChanged(m_settings, settings);
    m_baseChanged          = (m_settings.base != settings.base);

    // сохраняем старые и применяем новые
    m_prevSettings = m_settings;                                          // прошлые
    m_settings     = settings;                                            // текущие

    // если база изменилась и есть данные — пересчитать все значения и направления
    if (m_baseChanged && !m_groups.isEmpty()) {
        CalculateMesurement::recalcAllGroups(                              // пересчёт чисел
            m_groups,
            m_settings.mode,
            m_settings.base,
            m_settings.step,
            m_settings.manualText,
            m_settings.formula,
            m_settings.formulaCount
        );
        CalculateMesurement::recalcDirectionsInGroups(                     // пересчёт направлений
            m_groups,
            /*maxStepForBidi*/ m_settings.count,
            /*eps*/ 1e-4
        );
        m_baseChanged = false;                                             // сбрасываем флаг базы
    }
}

// выбрать действие для add
DataMeasurement::AddAction DataMeasurement::logicalAdd() const
{
    // если нет ни одной группы
    if (m_groups.isEmpty())
        return AddAction::NewGroup;                                        // создаём первую группу

    // если поменялась структура шагов
    if (m_stepStructureChanged)
        return AddAction::NewGroup;                                        // начинаем новую группу

    // если однонаправленно и шаги закончились
    if (m_settings.mode != StepMode::None
        && !m_settings.bidirectional
        && m_currentStep >= m_settings.count)
        return AddAction::NewGroup;                                        // открываем новую группу

    // если в шаге ещё есть повторы
    if (m_currentRepeat < m_settings.repeatCount)
        return AddAction::ContinueStep;                                    // просто дописываем повтор

    // иначе пора открывать новый шаг
    return AddAction::NewStep;                                             // старт нового шага
}



// добавить новое значение
void DataMeasurement::add(double value)
{
    AddAction what = logicalAdd();                                         // решаем что делать

    switch (what) {                                                        // выполняем действие
        case AddAction::NewGroup:
            startNewGroup(value);                                          // группа → шаг → измерение
            m_stepStructureChanged = false;                                 // сбрасываем флаг структуры
            return;
        case AddAction::NewStep:
            startNewStep(value);                                           // стартуем новый шаг
            break;
        case AddAction::ContinueStep:
            addNewMeasurement(value);                                      // дописываем повтор
            break;
    }
}

// создать новую группу и сразу начать шаг
void DataMeasurement::startNewGroup(double firstValue)
{

    ++m_groupIdCounter;                                                    // увеличиваем id                                                   // сбрасываем номер шага
    m_currentRepeat = 0;                                                   // сбрасываем повтор
    m_currentStep = 0;

    MeasurementGroup g;                                                    // создаём группу
    g.groupId = m_groupIdCounter;                                          // проставляем id
    g.mode    = m_settings.mode;                                           // копируем режим
    g.type    = m_settings.bidirectional
              ? MeasurementGroupType::Bidirectional
              : MeasurementGroupType::Unidirectional;                      // ставим тип

    m_groups.append(g);                                                    // сохраняем группу
    startNewStep(firstValue);                                              // создаём первый шаг
}

// выбрать действие для шага
DataMeasurement::StepAction DataMeasurement::logicalStartNewStep() const
{

    // если шага ещё не было
    if (m_groups.isEmpty() || currentGroup().steps.isEmpty()|| m_currentStep == 0)
        return StepAction::StartStep;                                      // начинаем с 1

    // если ограничений по шагам нет
    if (m_settings.mode == StepMode::None)
        return StepAction::ContinueStepPlus;                               // растём всегда

    // если однонаправленно — просто +1
    if (!m_settings.bidirectional)
        return StepAction::ContinueStepPlus;                               // без разворотов

    // ниже — логика пинг‑понга
    const int N = m_settings.count;                                        // крайний шаг
    const int cur = m_currentStep;                                         // текущий номер
    const auto& steps = currentGroup().steps;                              // ссылка на шаги
    const int prev  = steps.isEmpty() ? 0 : steps.last().stepNumber;       // предыдущий номер
    const int prev2 = steps.size() >= 2 ? steps[steps.size() - 2].stepNumber : 0; // предпредыдущий

    // если количество шагов не задано
    if (N <= 0)
        return StepAction::ContinueStepPlus;                               // как без лимита

    // левая граница: сначала дубль 1, затем вправо
    if (cur <= 1  && !steps.isEmpty()) {
        if (prev2 == 0)                 return StepAction::ContinueStepPlus;  // старт группы: 1 → 2
        if (prev == 1 && prev2 == 1) return StepAction::ContinueStepPlus; // после дубля 1 → вправо
        if (prev == 1)                 return StepAction::ContinueStepConst; // первое касание 1 → дубль
        return StepAction::ContinueStepConst; // на всякий случай (если cur<=1, а prev!=1)
    }

    // правая граница: сначала дубль N, затем влево
    if (cur >= N) {
        if (prev == N && prev2 == N) return StepAction::ContinueStepMinus; // после дубля N → влево
        if (prev == N)               return StepAction::ContinueStepConst; // первое касание N → дубль
        return StepAction::ContinueStepConst; // на всякий случай
    }

    // внутри диапазона продолжаем предыдущее направление
    if (prev2 == 0) return StepAction::ContinueStepPlus;                   // истории нет → вправо
    if (prev2 < prev) return StepAction::ContinueStepPlus;                 // росли → вправо
    if (prev2 > prev) return StepAction::ContinueStepMinus;                // падали → влево

    // запасной случай
    return StepAction::ContinueStepPlus;                                   // по умолчанию вправо
}

// начать новый шаг и записать первое значение
void DataMeasurement::startNewStep(double firstValue)
{
    m_currentRepeat = 0;                                                   // сбрасываем повторы

    StepAction act = logicalStartNewStep();                                // выбираем действие для шага

    switch (act) {                                                         // меняем номер шага
        case StepAction::StartStep:
            m_currentStep = 1;                                             // ставим 1
            break;
        case StepAction::ContinueStepPlus:
            ++m_currentStep;                                               // плюс один
            break;
        case StepAction::ContinueStepMinus:
            --m_currentStep;                                               // минус один
            if (m_currentStep < 1) m_currentStep = 1;                      // защита от нуля
            break;
        case StepAction::ContinueStepConst:
            /* номер шага тот же */                                        // оставляем как есть
            break;
    }

    MeasurementSeries series;                                              // создаём шаг
    series.stepNumber = m_currentStep;                                     // проставляем номер
    series.direction  = ApproachDirection::Unknown;                        // направление определим позже

    currentGroup().steps.append(series);                                   // кладём шаг в группу
    addNewMeasurement(firstValue);                                         // записываем первое измерение

    // после первого измерения можно определить направление
    if (currentGroup().steps.size() >= 2) {
        const auto& prevS = currentGroup().steps[currentGroup().steps.size() - 2]; // предыдущий шаг
        const auto& currS = currentSeries();                                        // текущий шаг
        const bool isBidi = (currentGroup().type == MeasurementGroupType::Bidirectional); // флаг bidi
        auto dir = CalculateMesurement::determineSeriesDirection(                    // единый метод
                     prevS, currS, isBidi, /*maxStep*/ m_settings.count, /*eps*/1e-4);
        currentSeries().direction = dir;                                             // записываем направление
    }
}

// добавить одно измерение в текущий шаг
void DataMeasurement::addNewMeasurement(double value)
{
    MeasurementSeries& series = currentSeries();                         // берём текущий шаг

    ++m_currentRepeat;                                                   // увеличиваем номер повтора

    auto v = CalculateMesurement::compute(                               // считаем три значения
        /*raw*/         value,
        /*base*/        m_settings.base,
        /*mode*/        m_settings.mode,
        /*stepNumber*/  series.stepNumber,
        /*uniformStep*/ m_settings.step,
        /*manualText*/  m_settings.manualText,
        /*formula*/     m_settings.formula,
        /*formulaCnt*/  m_settings.formulaCount
    );

    Measurement m;                                                       // собираем запись
    m.repeatIndex = m_currentRepeat;                                     // номер повтора
    m.raw         = value;                                               // сырое значение
    m.distance    = v.distance;                                          // смещение
    m.expected    = v.expected;                                          // ожидаемое
    m.deviation   = v.deviation;                                         // погрешность

    series.measurements.append(m);                                       // сохраняем запись
}

// загрузить группы извне
void DataMeasurement::setGroups(const QVector<MeasurementGroup>& groups)
{
    m_groups = groups;                                                   // копируем склад
    int maxId = 0;                                                       // ищем максимальный id
    for (const auto& g : m_groups) maxId = std::max(maxId, g.groupId);   // обновляем максимум
    m_groupIdCounter = maxId;                                            // ставим счётчик
    m_currentStep = 0;                                                   // сбрасываем шаг
    m_currentRepeat = 0;                                                 // сбрасываем повтор
}

// очистить всё
void DataMeasurement::clear()
{
    m_groups.clear();                                                    // чистим склад
    m_currentStep = 0;                                                   // сбрасываем шаг
    m_currentRepeat = 0;                                                 // сбрасываем повтор
    m_groupIdCounter = 0;                                                // сбрасываем счётчик
    m_stepStructureChanged = false;                                      // сбрасываем флаг
    m_baseChanged = false;                                               // сбрасываем флаг
    m_prevSettings = StepSettings{};                                     // сбрасываем прошлые
    m_settings     = StepSettings{};                                     // сбрасываем текущие
}

// доступ к последней группе (non-const)
MeasurementGroup& DataMeasurement::currentGroup()
{
    return m_groups.last();
}

// доступ к последнему шагу (non-const)
MeasurementSeries& DataMeasurement::currentSeries()
{
    return m_groups.last().steps.last();
}

// доступ к последней группе (const)
const MeasurementGroup& DataMeasurement::currentGroup() const
{
    return m_groups.last();
}

// доступ к последнему шагу (const)
const MeasurementSeries& DataMeasurement::currentSeries() const
{
    return m_groups.last().steps.last();
}

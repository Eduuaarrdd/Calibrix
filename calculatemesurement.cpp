#include "calculatemesurement.h"        // заголовок калькулятора
#include <QStringList>                   // разбиение строки
#include <QRegularExpression>            // разделители
#include <cmath>                         // isnan/abs
#include <algorithm>                     // std::max

// -------------------------------------------------------
// БЛОК 1. ПУБЛИЧНЫЕ РАСЧЁТЫ
// -------------------------------------------------------

double CalculateMesurement::distance(double raw, double base)
{
    return raw - base;                   // простая разница
}

double CalculateMesurement::deviation(double d, double e)
{
    if (std::isnan(e)) return std::numeric_limits<double>::quiet_NaN(); // если ожидаемого нет
    return d - e;                         // отклонение от ожидаемого
}

double CalculateMesurement::expected(StepMode mode,
                                     int stepNumber,
                                     double uniformStep,
                                     const QString& manualText,
                                     const QString& formula,
                                     int n)
{
    switch (mode) {                       // выбираем ветку по режиму
        case StepMode::Uniform: return expectedUniform(stepNumber, uniformStep); // равномерно
        case StepMode::Manual:  return expectedManual (stepNumber, manualText);  // вручную
        case StepMode::Formula: return expectedFormula(stepNumber, formula, n);  // формула
        case StepMode::None:
        default:                return std::numeric_limits<double>::quiet_NaN(); // нет ожидаемого
    }
}

double CalculateMesurement::expectedUniform(int stepNumber, double step)
{
    if (stepNumber <= 0) return 0.0;     // защита от нуля и ниже
    return stepNumber * step;             // линейная сетка
}

double CalculateMesurement::expectedManual(int stepNumber, const QString& text)
{
    const QVector<double> list = parseManual(text);           // парсим список значений
    if (stepNumber <= 0) return 0.0;                          // защита от нуля и ниже
    if (stepNumber > list.size()) return 0.0;                 // выход за границы даёт 0.0
    return list[stepNumber - 1];                              // берём нужный элемент
}

double CalculateMesurement::expectedFormula(int /*stepNumber*/, const QString& /*f*/, int /*n*/)
{
    return 0.0;                            // заглушка под формулы
}

CalculateMesurement::Values
CalculateMesurement::compute(double raw,
                             double base,
                             StepMode mode,
                             int    stepNumber,
                             double uniformStep,
                             const QString& manualText,
                             const QString& formula,
                             int    formulaCount)
{
    Values v;                              // собираем пакет значений
    v.distance  = distance(raw, base);     // смещение от базы
    v.expected  = expected(mode, stepNumber, uniformStep, manualText, formula, formulaCount); // ожидаемое
    v.deviation = deviation(v.distance, v.expected); // погрешность
    return v;                              // возвращаем пакет
}

ApproachDirection
CalculateMesurement::determineSeriesDirection(const MeasurementSeries& prevSeries,
                                              const MeasurementSeries& currSeries,
                                              bool bidirectional,
                                              int maxStepForBidi,
                                              double eps)
{
    if (prevSeries.measurements.isEmpty() || currSeries.measurements.isEmpty())
        return ApproachDirection::Unknown; // если нет данных

    if (bidirectional) {                   // логика для пинг-понга
        const int prevN = prevSeries.stepNumber;              // прошлый номер шага
        const int currN = currSeries.stepNumber;              // текущий номер шага

        if (currN > prevN) return ApproachDirection::Forward; // рост номера = вперёд
        if (currN < prevN) return ApproachDirection::Backward;// падение номера = назад

        if (maxStepForBidi > 0) {                             // обработка дублей на границах
            if (currN == maxStepForBidi) return ApproachDirection::Backward; // N->N = назад
            if (currN == 1)             return ApproachDirection::Forward;   // 1->1 = вперёд
        }

        const double p = averageRaw(prevSeries);              // среднее прошлого шага
        const double c = averageRaw(currSeries);              // среднее текущего шага
        return deltaToDirection(c - p, eps);                  // fallback по данным
    }

    const double prevAvg = averageRaw(prevSeries);            // среднее прошлого шага
    const double currAvg = averageRaw(currSeries);            // среднее текущего шага
    return deltaToDirection(currAvg - prevAvg, eps);          // знак дельты задаёт направление
}

// -------------------------------------------------------
// БЛОК 2. ПУБЛИЧНЫЕ ПЕРЕСЧЁТЫ
// -------------------------------------------------------

void CalculateMesurement::recalcAllGroups(QVector<MeasurementGroup>& groups,
                                          StepMode mode,
                                          double base,
                                          double uniformStep,
                                          const QString& manualText,
                                          const QString& formula,
                                          int    n)
{
    QVector<double> manualList;            // кэш значений для Manual
    if (mode == StepMode::Manual) manualList = parseManual(manualText); // парсим один раз

    auto expectedFor = [&](int stepNumber)->double {          // быстрый выбор ожидаемого
        switch (mode) {
            case StepMode::Uniform: return expectedUniform(stepNumber, uniformStep); // равномерно
            case StepMode::Manual:  {
                if (stepNumber <= 0) return 0.0;                                // защита
                if (stepNumber > manualList.size()) return 0.0;                 // границы
                return manualList[stepNumber - 1];                               // значение
            }
            case StepMode::Formula: return expectedFormula(stepNumber, formula, n); // формула
            case StepMode::None:
            default:                return std::numeric_limits<double>::quiet_NaN(); // нет ожидаемого
        }
    };

    for (auto& g : groups) {                       // идём по группам
        for (auto& s : g.steps) {                  // идём по шагам
            const double exp = expectedFor(s.stepNumber);     // ожидаемое шага
            for (auto& m : s.measurements) {       // идём по измерениям
                m.distance  = distance(m.raw, base);          // смещение от базы
                m.expected  = exp;                            // ожидаемое по шагу
                m.deviation = deviation(m.distance, m.expected); // погрешность
            }
        }
    }
}

void CalculateMesurement::recalcDirectionsInGroups(QVector<MeasurementGroup>& groups,
                                                   int maxStepForBidi,
                                                   double eps)
{
    for (auto& g : groups) {                        // идём по группам
        if (g.steps.isEmpty()) continue;            // пропускаем пустые

        g.steps[0].direction = ApproachDirection::Unknown; // первый шаг = Unknown

        const bool isBidi = (g.type == MeasurementGroupType::Bidirectional); // флаг bidi

        for (int i = 1; i < g.steps.size(); ++i) {  // по остальным шагам
            const auto d = determineSeriesDirection(g.steps[i - 1], g.steps[i],
                                                    isBidi, maxStepForBidi, eps); // единый метод
            g.steps[i].direction = d;               // пишем направление
        }
    }
}

// -------------------------------------------------------
// БЛОК 3. ПАРСЕР
// -------------------------------------------------------

QVector<double> CalculateMesurement::parseManual(const QString& text)
{
    if (text.isEmpty()) return {};       // если пусто, выходим

    static const QRegularExpression sepRe(R"([,;\s]+)");     // общие разделители
    const QStringList toks = text.split(sepRe, Qt::SkipEmptyParts); // режем строку

    QVector<double> out;                 // итоговый список
    out.reserve(toks.size());            // резервируем память

    for (const auto& t : toks) {         // обходим токены
        bool ok = false;                 // флаг парсинга
        const double v = t.toDouble(&ok);// пробуем в double
        if (ok) out.push_back(v);        // добавляем число
        // если не число — пропускаем
    }
    return out;                          // отдаём список значений
}

int CalculateMesurement::manualCount(const QString& text)
{
    return parseManual(text).size();     // количество значений
}

// -------------------------------------------------------
// БЛОК 4. ПРИВАТНЫЕ ПОМОЩНИКИ
// -------------------------------------------------------

double CalculateMesurement::averageRaw(const MeasurementSeries& s)
{
    if (s.measurements.isEmpty())
        return std::numeric_limits<double>::quiet_NaN();     // если пусто

    double sum = 0.0;                   // сумма значений
    for (const auto& m : s.measurements) sum += m.raw;       // накапливаем

    return sum / s.measurements.size(); // среднее по шагу
}

ApproachDirection
CalculateMesurement::deltaToDirection(double delta, double eps)
{
    if (delta >  eps) return ApproachDirection::Forward;     // рост = вперёд
    if (delta < -eps) return ApproachDirection::Backward;    // падение = назад
    return ApproachDirection::Unknown;                        // мелочь = неясно
}

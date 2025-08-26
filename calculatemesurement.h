#ifndef CALCULATEMESUREMENT_H
#define CALCULATEMESUREMENT_H

#include <QVector>                      // контейнеры Qt
#include <QString>                      // строки Qt
#include <limits>                       // NaN/Inf
#include "typemeasurement.h"            // общие типы и StepMode

// независимый калькулятор без доступа к настройкам и складу
class CalculateMesurement
{
public:
    // компактная пачка трёх значений
    struct Values {
        double distance  = 0.0;                                         // смещение от базы
        double expected  = std::numeric_limits<double>::quiet_NaN();    // ожидаемое по шагу
        double deviation = std::numeric_limits<double>::quiet_NaN();    // погрешность = distance - expected
    };

    // -------------------------------------------------------
    // БЛОК 1. ПУБЛИЧНЫЕ РАСЧЁТЫ
    // -------------------------------------------------------

    static double distance(double raw, double base);                    // считаем смещение
    static double deviation(double distance, double expected);          // считаем погрешность

    static double expected(StepMode mode,
                           int stepNumber,
                           double uniformStep,
                           const QString& manualText,
                           const QString& formula,
                           int formulaCount);                           // считаем ожидаемое

    static double expectedUniform(int stepNumber, double step);         // ожидаемое для Uniform
    static double expectedManual (int stepNumber, const QString& text); // ожидаемое для Manual
    static double expectedFormula(int stepNumber, const QString& f, int n); // ожидаемое для Formula

    static Values compute(double raw,
                          double base,
                          StepMode mode,
                          int    stepNumber,
                          double uniformStep,
                          const QString& manualText,
                          const QString& formula,
                          int    formulaCount);                         // считаем всё разом

    static ApproachDirection determineSeriesDirection(const MeasurementSeries& prevSeries,
                                                      const MeasurementSeries& currSeries,
                                                      bool bidirectional,
                                                      int maxStepForBidi,
                                                      double eps = 1e-4); // единая логика направления

    // -------------------------------------------------------
    // БЛОК 2. ПУБЛИЧНЫЕ ПЕРЕСЧЁТЫ
    // -------------------------------------------------------

    static void recalcAllGroups(QVector<MeasurementGroup>& groups,
                                StepMode mode,
                                double base,
                                double uniformStep,
                                const QString& manualText,
                                const QString& formula,
                                int    formulaCount);                   // пересчёт чисел по всем

    static void recalcDirectionsInGroups(QVector<MeasurementGroup>& groups,
                                         int maxStepForBidi,
                                         double eps = 1e-4);            // пересчёт направлений по всем

    // -------------------------------------------------------
    // БЛОК 3. ПАРСЕР
    // -------------------------------------------------------

    static QVector<double> parseManual(const QString& manualText);      // парсим Manual в список
    static int             manualCount(const QString& manualText);      // считаем элементы Manual

private:
    // -------------------------------------------------------
    // БЛОК 4. ПРИВАТНЫЕ ПОМОЩНИКИ
    // -------------------------------------------------------

    static double averageRaw(const MeasurementSeries& s);               // среднее raw по шагу
    static ApproachDirection deltaToDirection(double delta, double eps);// дельта -> направление
};

#endif // CALCULATEMESUREMENT_H

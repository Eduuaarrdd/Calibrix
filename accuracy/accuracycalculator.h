#ifndef ACCURACYCALCULATOR_H
#define ACCURACYCALCULATOR_H

#include "datameasurement.h"
#include <QVector>
#include <limits>

// Структура результатов расчёта по ГОСТ ISO 230-2—2016
struct AccuracyResult
{
    int stepNumber = 0;

    double meanForward = 0.0;            // x⁺
    double meanBackward = 0.0;           // x⁻
    double meanBidirectional = 0.0;      // x̄ᵢ
    double reversalError = 0.0;          // Bᵢ

    double stddevForward = 0.0;          // s⁺
    double stddevBackward = 0.0;         // s⁻

    double repeatabilityForward = 0.0;   // R⁺ = 4s⁺
    double repeatabilityBackward = 0.0;  // R⁻ = 4s⁻
    double repeatabilityBidirectional = 0.0; // Ri

    double systematicError = 0.0;        // E
    double meanRange = 0.0;              // M
    double positioningAccuracy = 0.0;    // A

    // ——— Новое поле: ожидаемое значение позиции ———
    double expectedPosition = std::numeric_limits<double>::quiet_NaN();
};

using AccuracyResultList = QVector<AccuracyResult>;

// ─────────────────────────────────────────────
// Класс расчёта точности по ГОСТ ISO 230-2—2016
// ─────────────────────────────────────────────

class AccuracyCalculator
{
public:
    // Главный метод: принимает данные и номера групп
    static AccuracyResultList compute(const DataMeasurement& measurement);

private:
    // Эталонный расчёт — по ГОСТ ISO 230-2 для всех шагов группы
    static AccuracyResultList computeStrictBidirectional(const MeasurementGroup& group);

};

#endif // ACCURACYCALCULATOR_H


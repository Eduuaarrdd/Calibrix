#include "accuracycalculator.h"
#include <QtMath>
#include <algorithm>

// ─────────────────────────────────────────────────────
// Публичный метод: точка входа
// ─────────────────────────────────────────────────────
AccuracyResultList AccuracyCalculator::compute(const DataMeasurement& measurement)
{
    AccuracyResultList allResults;

    // ❗ Здесь в будущем будет вызов валидатора корректности данных

    for (const auto& group : measurement.groups()) {
        if (!group.selectedFor)
            continue;

          // Вызываем строгий расчёт только для одной группы
          AccuracyResultList results = computeStrictBidirectional(group);

          // Добавляем результат в список
          allResults += results;
      }

      return allResults;
}

AccuracyResultList AccuracyCalculator::computeStrictBidirectional(const MeasurementGroup& group)
{
    AccuracyResultList results;

    // ─────────────────────────────────────
    // 1. Разделение серий по шагам и направлениям
    // ─────────────────────────────────────
    QMap<int, QVector<const MeasurementSeries*>> forwardMap;
    QMap<int, QVector<const MeasurementSeries*>> backwardMap;

    for (const auto& s : group.steps) {
        if (s.measurements.isEmpty())
            continue;

        if (s.direction == ApproachDirection::Forward)
            forwardMap[s.stepNumber].append(&s);
        else if (s.direction == ApproachDirection::Backward)
            backwardMap[s.stepNumber].append(&s);
    }

    // ─────────────────────────────────────
    // 2. Расчёт по каждому шагу
    // ─────────────────────────────────────
    for (int step : forwardMap.keys()) {
        if (!backwardMap.contains(step))
            continue;

        const auto& fSeries = forwardMap[step];
        const auto& bSeries = backwardMap[step];

        QVector<double> fValues, bValues;
        double expected = std::numeric_limits<double>::quiet_NaN();

        for (const auto* s : fSeries) {
            for (const auto& m : s->measurements)
                if (!std::isnan(m.deviation))
                    fValues.append(m.deviation);

            if (!s->measurements.isEmpty() && std::isnan(expected) == true && !std::isnan(s->measurements.first().expected))
                expected = s->measurements.first().expected;
        }

        for (const auto* s : bSeries) {
            for (const auto& m : s->measurements)
                if (!std::isnan(m.deviation))
                    bValues.append(m.deviation);
        }

        if (fValues.isEmpty() || bValues.isEmpty())
            continue;

        AccuracyResult r;
        r.stepNumber = step;
        r.expectedPosition = expected;

        auto mean = [](const QVector<double>& v) {
            return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        };
        auto stddev = [](const QVector<double>& v, double m) {
            if (v.size() < 2) return 0.0;
            double sum = 0.0;
            for (double x : v) sum += qPow(x - m, 2);
            return qSqrt(sum / (v.size() - 1));
        };

        r.meanForward  = mean(fValues);
        r.meanBackward = mean(bValues);
        r.meanBidirectional = (r.meanForward + r.meanBackward) / 2.0;
        r.reversalError     = r.meanForward - r.meanBackward;

        r.stddevForward  = stddev(fValues, r.meanForward);
        r.stddevBackward = stddev(bValues, r.meanBackward);

        r.repeatabilityForward  = 4.0 * r.stddevForward;
        r.repeatabilityBackward = 4.0 * r.stddevBackward;
        r.repeatabilityBidirectional = std::max({
            r.repeatabilityForward,
            r.repeatabilityBackward,
            2.0 * r.stddevForward + 2.0 * r.stddevBackward + qAbs(r.reversalError)
        });

        r.systematicError = qAbs(r.reversalError);
        r.positioningAccuracy = std::max(
            r.meanForward + 2.0 * r.stddevBackward,
            r.meanBackward + 2.0 * r.stddevForward)
            -
            std::min(
            r.meanForward - 2.0 * r.stddevBackward,
            r.meanBackward - 2.0 * r.stddevForward
        );

        // пока meanRange = 0.0, т.к. считается потом
        results.append(r);
    }

    // ─────────────────────────────────────
    // 3. Интегральные параметры (по всей группе)
    // ─────────────────────────────────────
    if (!results.isEmpty()) {
        AccuracyResult total;
        total.stepNumber = -1;

        // E — диапазон средних значений
        double minMean = results.first().meanBidirectional;
        double maxMean = results.first().meanBidirectional;
        double maxB    = 0.0;
        double maxR    = 0.0;
        double maxPos  = std::numeric_limits<double>::lowest();
        double minPos  = std::numeric_limits<double>::max();

        for (const auto& r : results) {
            minMean = std::min(minMean, r.meanBidirectional);
            maxMean = std::max(maxMean, r.meanBidirectional);
            maxB    = std::max(maxB, std::abs(r.reversalError));
            maxR    = std::max(maxR, r.repeatabilityBidirectional);

            double hi = std::max(
                r.meanForward + 2.0 * r.stddevBackward,
                r.meanBackward + 2.0 * r.stddevForward);
            double lo = std::min(
                r.meanForward - 2.0 * r.stddevBackward,
                r.meanBackward - 2.0 * r.stddevForward);

            maxPos = std::max(maxPos, hi);
            minPos = std::min(minPos, lo);
        }

        total.meanRange            = maxMean - minMean; // E
        total.systematicError      = maxB;              // M
        total.repeatabilityBidirectional = maxR;        // Rmax
        total.positioningAccuracy  = maxPos - minPos;   // A

        results.append(total);
    }

    return results;
}





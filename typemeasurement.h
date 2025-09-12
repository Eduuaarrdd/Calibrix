#ifndef TYPEMEASUREMENT_H
#define TYPEMEASUREMENT_H

#include <QVector>

enum class ApproachDirection {
    Unknown,
    Forward,
    Backward
};

struct Measurement {
    int repeatIndex;     // Индекс повтора внутри шага (с 1)
    double raw;          // Отфильтрованное значение
    double distance;     // Смещение относительно базовой точки (raw - base)
    double deviation;    // Отклонение: distance - expected
};

struct MeasurementSeries {
    int stepNumber;      // Порядковый номер шага
    double expected;     // Теоретическое значение шага
    ApproachDirection direction = ApproachDirection::Unknown; // направление шага
    QVector<Measurement> measurements;        // Массив измерений для этого шага
};

struct MeasurementGroup {
    int groupId = 1;
    bool selectedFor = true;
    bool biditype = true;
    QVector<MeasurementSeries> steps;
};

#endif // TYPEMEASUREMENT_H

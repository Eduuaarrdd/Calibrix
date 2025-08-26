#ifndef TYPEMEASUREMENT_H
#define TYPEMEASUREMENT_H

#include <QVector>
#include <limits>
#include "settingsmanager.h"

enum class ApproachDirection {
    Unknown,
    Forward,
    Backward
};

enum class MeasurementGroupType {
    Unidirectional,
    Bidirectional
};

struct Measurement {
    int repeatIndex;     // Индекс повтора внутри шага (с 1)
    double raw;          // Отфильтрованное значение
    double distance;     // Смещение относительно базовой точки (raw - base)
    double expected;     // Теоретическое значение шага
    double deviation;    // Отклонение: distance - expected
};

struct MeasurementSeries {
    int stepNumber;                           // Порядковый номер шага
    QVector<Measurement> measurements;        // Повторы для этого шага
    ApproachDirection direction = ApproachDirection::Unknown; // направление шага
};

struct MeasurementGroup {
    int groupId = 1;
    StepMode mode = StepMode::None;  // Режим шага (Uniform, Manual, Formula, None)
    MeasurementGroupType type = MeasurementGroupType::Unidirectional;
    bool selectedFor = true;
    QVector<MeasurementSeries> steps;
};

#endif // TYPEMEASUREMENT_H

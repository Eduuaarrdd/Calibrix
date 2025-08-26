#ifndef ACCURACYDATASAVER_H
#define ACCURACYDATASAVER_H

#include "datameasurement.h"
#include "accuracyvisualizer.h"

// ─────────────────────────────────────────────────────
// Класс преобразования модели таблицы в DataMeasurement
// ─────────────────────────────────────────────────────

class AccuracyDataSaver
{
public:
    // Основной метод: преобразует TableModel в DataMeasurement
    static DataMeasurement extractFromModel(const QVector<TableRow>& tableModel);
};

#endif // ACCURACYDATASAVER_H


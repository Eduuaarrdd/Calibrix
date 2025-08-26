#include "accuracydatasaver.h"

// ─────────────────────────────────────────────────────
// Построение структуры DataMeasurement по модели таблицы
// ─────────────────────────────────────────────────────
DataMeasurement AccuracyDataSaver::extractFromModel(const QVector<TableRow>& tableModel)

{
    DataMeasurement result;
    QVector<MeasurementGroup> groups;

    MeasurementGroup currentGroup;
    int lastGroupId = -1;

    // ───── Проход по строкам слепка ─────
    for (const auto& row : tableModel)
    {
        switch (row.type)
        {
        // ───── Заголовок группы ─────
        case TableRow::Type::GroupHeader:
        {
            if (!currentGroup.steps.isEmpty())
                groups.append(currentGroup);

            currentGroup = {};
            currentGroup.groupId = row.groupId;
            currentGroup.type = row.isBidirectional
                ? MeasurementGroupType::Bidirectional
                : MeasurementGroupType::Unidirectional;
            currentGroup.mode = row.mode;
            currentGroup.selectedFor = row.selectedFor;
            lastGroupId = row.groupId;
            break;
        }

        // ───── Строка измерения ─────
        case TableRow::Type::Measurement:
        {
            // Пропускаем строки без значения дистанции
            if (qIsNaN(row.distance))
                break;

            if (row.groupId != lastGroupId) {
                if (!currentGroup.steps.isEmpty())
                    groups.append(currentGroup);

                currentGroup = {};
                currentGroup.groupId = row.groupId;
                currentGroup.type = row.isBidirectional
                    ? MeasurementGroupType::Bidirectional
                    : MeasurementGroupType::Unidirectional;
                currentGroup.mode = row.mode;
                lastGroupId = row.groupId;
            }

            // ——— Формируем MeasurementSeries напрямую из строки ———
            MeasurementSeries step;
            step.stepNumber = row.stepNumber;
            step.direction = row.direction;

            Measurement m;
            m.repeatIndex = row.repeatIndex;
            m.distance = row.distance;
            m.expected = row.expected;
            m.deviation = row.deviation;
            m.raw = row.distance;

            step.measurements.append(m);
            currentGroup.steps.append(step);

            break;
        }

        // ───── Служебные строки ─────
        case TableRow::Type::AddRowButton:
        case TableRow::Type::AddGroupButton:
            break;
        }
    }

    // ───── Финальный пуш группы ─────
    if (!currentGroup.steps.isEmpty())
        groups.append(currentGroup);

    result.setGroups(groups);
    return result;
}




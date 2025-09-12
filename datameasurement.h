#ifndef DATAMEASUREMENT_H
#define DATAMEASUREMENT_H

#include <QVector>
#include <QSharedPointer>
#include <limits>
#include "typemeasurement.h"
#include "plangenerator.h"

// Исполнитель плана: хранит группы/серии/измерения и записывает новые значения по PlanSave
class DataMeasurement
{
public:
    DataMeasurement() = default;

    // Применить (или пере-применить) план
    void applyPlanSave(QSharedPointer<const PlanSave> plan);

    // Добавить одно измерение (сырое значение после фильтра)
    void add(double value);

    // Очистить всё
    void clear();

    // Внешняя загрузка групп (например, из файла)
    void setGroups(const QVector<MeasurementGroup>& groups);

    // Доступ к складу
    const QVector<MeasurementGroup>& groups() const { return m_groups; }

private:
    // ------------------------
    // Снимок текущего элемента плана для выполнения
    // ------------------------
    struct ElementSnapshot {
        SaveAction        action      = SaveAction::Measurement;
        int               stepNumber  = PlanGenConst::kStepNone;
        double            expected    = std::numeric_limits<double>::quiet_NaN();
        int               address     = 0;
        ApproachDirection direction   = ApproachDirection::Unknown;
        double            value       = 0.0;  // сырое входное значение для записи
    };

    ElementSnapshot m_snap; // единственный снимок текущего элемента плана

    // ------------------------
    // Внутреннее состояние
    // ------------------------
    QVector<MeasurementGroup>      m_groups;             // склад
    int                            m_groupIdCounter = 0; // автонумерация групп

    QSharedPointer<const PlanSave> m_plan;               // активный план
    int                            m_cursor = 0;         // индекс текущего элемента плана
    bool                           m_firstPass = true;   // первый проход после смены структуры

    quint64                        m_prevStructureHash = 0;  // для change=2
    double                         m_prevBase = std::numeric_limits<double>::quiet_NaN(); // для change=1

    // ------------------------
    // Основные шаги конвейера
    // ------------------------
    int  detectChange() const;                        // 0 — нет, 1 — изменилась база, 2 — изменилась структура
    void takeSnapshot(double value);                  // заполнить снимок из m_plan[m_cursor] (с учётом NONE/firstPass)
    void advanceCursor();                             // сдвинуть курсор и управлять m_firstPass

    // Операции по снимку
    void startNewGroup();     // создать группу
    void startNewStep();      // создать серию
    void addNewMeasurement(); // добавить измерение

    // Пересчёт при смене базы (только текущая группа)
    void recalcCurrentGroupByBaseAndPlan();

    // Хелперы по текущим контейнерам
    bool hasGroup() const { return !m_groups.isEmpty(); }
    MeasurementGroup& currentGroup() { return m_groups.last(); }
    const MeasurementGroup& currentGroup() const { return m_groups.last(); }
    MeasurementSeries& currentSeries() { return m_groups.last().steps.last(); }
    const MeasurementSeries& currentSeries() const { return m_groups.last().steps.last(); }

    // Хелперы для NONE
    int  maxStepNumberInCurrentGroup() const;
    int  maxAddressInCurrentGroup() const;
};

#endif // DATAMEASUREMENT_H

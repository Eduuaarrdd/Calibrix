#ifndef DATAMEASUREMENT_H
#define DATAMEASUREMENT_H

#include <QVector>                      // контейнер для данных
#include <QString>                      // строки для настроек
#include "typemeasurement.h"            // Measurement*, Direction, Group types

// основное хранилище и логика добавления
class DataMeasurement
{
public:
    DataMeasurement() = default;                              // пустой конструктор

    void setStepSettings(const StepSettings& settings);       // поставить настройки
    bool isStepStructureChanged(const StepSettings& oldS,     // публичная проверка структуры
                                const StepSettings& newS) const;

    void add(double value);                                   // добавить новое значение
    void clear();                                             // очистить всё
    void setGroups(const QVector<MeasurementGroup>& groups);  // загрузить группы
    const QVector<MeasurementGroup>& groups() const { return m_groups; } // доступ к данным

private:
    // действия верхнего уровня (add)
    enum class AddAction { NewGroup, NewStep, ContinueStep }; // что делать в add
    // действия для шага (startNewStep)
    enum class StepAction { StartStep, ContinueStepPlus, ContinueStepMinus, ContinueStepConst }; // как менять номер шага

    // активные настройки
    StepSettings m_settings{};                                // текущие настройки
    StepSettings m_prevSettings{};                            // предыдущие настройки

    // склад измерений
    QVector<MeasurementGroup> m_groups;                       // все группы

    // состояние конвейера
    int  m_currentStep   = 0;                                 // текущий номер шага
    int  m_currentRepeat = 0;                                 // текущий повтор
    int  m_groupIdCounter = 0;                                // счётчик id групп

    // флаги изменений
    bool m_stepStructureChanged = false;                      // изменились поля структуры шагов
    bool m_baseChanged = false;                               // изменилась базовая точка

    // выбор действия для add
    AddAction  logicalAdd() const;                            // решить что делать в add
    // выбор действия для шага
    StepAction logicalStartNewStep() const;                   // решить как менять номер шага

    // создать новую группу и сразу начать шаг
    void startNewGroup(double firstValue);                    // создать группу и записать первое значение
    // начать новый шаг и записать первое значение
    void startNewStep(double firstValue);                     // создать шаг и записать значение
    // добавить один повтор в текущий шаг
    void addNewMeasurement(double value);                     // записать измерение

    // быстрый доступ к текущим элементам
    MeasurementGroup&  currentGroup();                        // последняя группа
    MeasurementSeries& currentSeries();                       // последний шаг
    const MeasurementGroup&  currentGroup() const;            // последняя группа
    const MeasurementSeries& currentSeries() const;           // последний шаг
};

#endif // DATAMEASUREMENT_H


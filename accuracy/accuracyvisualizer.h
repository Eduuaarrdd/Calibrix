#ifndef ACCURACYVISUALIZER_H
#define ACCURACYVISUALIZER_H

#include <QObject>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include "datameasurement.h"
#include "accuracycalculator.h"

#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QSizePolicy>

// ─────────────────────────────────────────────────────
// ВНЕШНЯЯ структура строки таблицы — теперь доступна везде
// ─────────────────────────────────────────────────────

struct TableRow {
    enum class Type {
        GroupHeader,
        Measurement,
        AddRowButton,
        AddGroupButton
    };

    Type type;
    int groupId = -1;

    // Только для заголовка группы
    bool isBidirectional = false;
    bool selectedFor = true;  // По умолчанию все группы выключены

    // Только для строки измерения
    int stepNumber = 0; //
    int repeatIndex = 0; //
    double distance = 0.0;
    double expected = 0.0;
    double deviation = 0.0;
    ApproachDirection direction = ApproachDirection::Unknown;
    StepMode mode = StepMode::None;
};

class AccuracyVisualizer : public QObject
{
    Q_OBJECT

public:
    explicit AccuracyVisualizer(QObject* parent = nullptr);

    // Установка виджетов (input table и таблицы с результатами)
    void setWidgets(QTableWidget* inputTable, QTableWidget* resultTableView);

    // Основная перерисовка таблицы из DataMeasurement
    void setTable(const DataMeasurement& measurement);

    // Отображает таблицу resultTable по результатам расчёта
    void setResultTable(const AccuracyResultList& results);

    // Основной метод: сохранить таблицу и вернуть копию слепка
    QVector<TableRow> prepareSnapshot();

    // ──────────────────────── Описание строки таблицы ────────────────────────
    QVector<TableRow> m_tableModel;

public slots:
    void addGroup();                     // Добавить новую пустую группу
    void addRowToGroup(int groupId);    // Добавить строку в существующую группу
    void deleteGroup(int groupId);      // Удалить группу со всеми её строками
    void deleteRow(int row);  // Удалить конкретное измерение

    void onCellEdited(int row, int column);  // Обработка редактирования значений
    void onCurrentCellChanged(int currentRow, int currentCol, int previousRow, int previousCol); // Сохраняем текст текущей ячейки перед редактированием

signals:
    // Сигналы управления от пользователя
    void deleteRowRequested(int row);
    void deleteGroupRequested(int groupId);
    void addRowToGroupRequested(int groupId);
    void addGroupRequested();

private:
    QTableWidget* inputTable = nullptr;
    QTableWidget* resultTable = nullptr;
    QString previousCellText;  // Предыдущее значение ячейки

    // ──────────────────────── Основные вспомогательные методы ────────────────────────

    // Отображение строки с заголовком группы (объединённая строка + кнопка удалить)
    void addGroupHeaderRow(int row, int groupId, bool isBidirectional, bool selected);

    // Отображение строки измерения
    void addMeasurementRow(int row, int stepNumber, int repeatIndex,
                           double distance, double expected, double deviation,
                           ApproachDirection direction, StepMode mode);

    // Добавить кнопку "Добавить строку" в группу
    void addAddRowButtonRow(int row, int groupId);

    // Добавить финальную кнопку "Добавить новую группу"
    void addGlobalAddGroupRow(int row);

    // Визуализировать таблицу из модели
    void renderTable();

    // Сохранить текущую таблицу (то, что видит пользователь) в m_tableModel
    void saveVisibleTable();

    // ──────────────────────── Компоненты построения строк ────────────────────────

    QTableWidgetItem* createReadonlyItem(double value);
    QTableWidgetItem* createReadonlyItem(const QString& text);// погрешность
    QComboBox* createDirectionComboBox(ApproachDirection dir);       // редактируемый выпадающий список направления
    QComboBox* createModeComboBox(StepMode mode);                    // редактируемый выпадающий список режима
    QPushButton* createDeleteButton();                               // кнопка удалить
    QPushButton* createAddRowButton();                               // кнопка ➕ Добавить строку
    QPushButton* createAddGroupButton();                             // кнопка ➕ Добавить группу

    // ──────────────────────── Утилиты ────────────────────────

    ApproachDirection directionFromString(const QString& str) const;
    StepMode modeFromString(const QString& str) const;


};

#endif // ACCURACYVISUALIZER_H

#include "accuracywindow.h"
#include "ui_accuracywindow.h"
#include "accuracy/accuracyvisualizer.h"
#include "accuracy/accuracydatasaver.h"
#include "accuracy/accuracycalculator.h"
#include <QMessageBox>


AccuracyWindow::AccuracyWindow(QWidget* parent)
    : QMainWindow(parent),
    ui(new Ui::AccuracyWindow),
    state(new AccuracyState(this)),
    visualizer(new AccuracyVisualizer(this)),
    exportManager(new ExportManager(this))
{
    ui->setupUi(this);

    visualizer->setWidgets(ui->inputTable, ui->resultTableView);

    //Изменение состояния окна
    connect(state, &AccuracyState::stateChanged, this, &AccuracyWindow::onStateChanged);

    //Кнопка добавить строку
    connect(visualizer, &AccuracyVisualizer::addRowToGroupRequested, this, &AccuracyWindow::onAddRowToGroup);

    //Кнопка добавить группу
    connect(visualizer, &AccuracyVisualizer::addGroupRequested, this, &AccuracyWindow::onAddGroup);

    //Кнопка удалить группу
    connect(visualizer, &AccuracyVisualizer::deleteGroupRequested, this, &AccuracyWindow::onDeleteGroup);

    //Кнопка удалить строку
    connect(visualizer, &AccuracyVisualizer::deleteRowRequested, this, &AccuracyWindow::onDeleteRowClicked);


    //Кнопка сохранить
    connect(ui->saveButton, &QPushButton::clicked, this, &AccuracyWindow::onSaveClicked);

    //Кнопка сброс
    connect(ui->resetButton, &QPushButton::clicked, this, &AccuracyWindow::onResetClicked);

    //Кнопка рассчета
    connect(ui->calculateButton, &QPushButton::clicked, this, &AccuracyWindow::onCalculateButtonClicked);

    //Редактирование ячейки
    connect(ui->inputTable, &QTableWidget::cellChanged, this, &AccuracyWindow::onCellEdited);

    //Привязка кнопки Экспорт
    connect(ui->exportButton, &QPushButton::clicked, this, &AccuracyWindow::onExportClicked);
}

AccuracyWindow::~AccuracyWindow()
{
    delete ui;
}

void AccuracyWindow::setMeasurementData(const DataMeasurement& copiedMeasurement, double base)
{
    if (copiedMeasurement.groups().isEmpty()) {
        QMessageBox::warning(this, "Ошибка данных", "Измерения отсутствуют.");
    }
    measurement = copiedMeasurement;
    basePoint = base;

    state->setState(AccuracyWindowState::Idle);
}


void AccuracyWindow::onStateChanged(AccuracyWindowState s)
{
    switch (s) {
    case AccuracyWindowState::Idle:
    {
        visualizer->setTable(measurement);
        break;
    }
    case AccuracyWindowState::Editing:
    {
        // TODO: логика редактирования
        break;
    }

    case AccuracyWindowState::Resetting:
    {
        visualizer->setTable(measurement);
        break;
    }

    case AccuracyWindowState::Saving:
    {
        // 1. Получаем копию слепка из визуализатора
        QVector<TableRow> snapshot = visualizer->prepareSnapshot();

        // 2. Парсим копию в новый DataMeasurement
        measurement = AccuracyDataSaver::extractFromModel(snapshot);

        // 3. Назад в Idle
        state->setState(AccuracyWindowState::Idle);
        break;
    }

    case AccuracyWindowState::Calculating:
    {
        // 1. Получаем актуальный слепок с таблицы
        QVector<TableRow> snapshot = visualizer->prepareSnapshot();

        // 2. Парсим в новый временный measurement
        DataMeasurement tempMeasurement = AccuracyDataSaver::extractFromModel(snapshot);

        // 3. Вызываем калькулятор
        AccuracyResultList results = AccuracyCalculator::compute(tempMeasurement);

        // 4. Показываем результаты пользователю
        visualizer->setResultTable(results);


        // 5. Сохраняем в history (можно потом использовать)
         //dataSaver.appendResult(tempMeasurement, results);

        // 6. сохранить локально для экспорта
        lastTempMeasurement = tempMeasurement;
        lastResults = results;

        // 7. Назад в Idle
        state->setState(AccuracyWindowState::Idle);
        break;
    }

    case AccuracyWindowState::Exporting:
    {
        // если calculating ещё не выполнялся — запретить экспорт
        if (lastResults.isEmpty()) {
            QMessageBox::warning(this, "Экспорт невозможен",
                                 "Сначала выполните расчёт (Кнопка «Рассчитать»).");
            state->setState(AccuracyWindowState::Idle);
            break;
        }

        // ok — экспорт по последним локально сохранённым данным
        exportManager->exportAccuracyCsv(this, lastTempMeasurement, lastResults);

        state->setState(AccuracyWindowState::Idle);
        break;
    }

    case AccuracyWindowState::Error:
    {
        QMessageBox::critical(this, "Ошибка", "Произошла ошибка при обработке данных.");
        break;
    }
    }
}

void AccuracyWindow::onAddRowToGroup(int groupId)
{
    visualizer->addRowToGroup(groupId);
    state->setState(AccuracyWindowState::Editing);
}

void AccuracyWindow::onAddGroup()
{
    visualizer->addGroup();
    state->setState(AccuracyWindowState::Editing);
}

void AccuracyWindow::onDeleteGroup(int groupId)
{
    visualizer->deleteGroup(groupId);
    state->setState(AccuracyWindowState::Editing);
}

void AccuracyWindow::onDeleteRowClicked(int row)
{
    visualizer->deleteRow(row);
    state->setState(AccuracyWindowState::Editing);
}

void AccuracyWindow::onSaveClicked()
{
    state->setState(AccuracyWindowState::Saving);
}

void AccuracyWindow::onResetClicked()
{
    state->setState(AccuracyWindowState::Resetting);
}

void AccuracyWindow::onCalculateButtonClicked()
{
    state->setState(AccuracyWindowState::Calculating);
}

void AccuracyWindow::onCellEdited(int row, int column)
{
    visualizer->onCellEdited(row, column);
    state->setState(AccuracyWindowState::Editing);
}

void AccuracyWindow::onExportClicked()
{
    state->setState(AccuracyWindowState::Exporting);
}

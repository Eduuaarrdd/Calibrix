#ifndef DATAVISUALIZER_H
#define DATAVISUALIZER_H

#include <QObject>
#include <QGridLayout>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QScatterSeries>
#include <QTableView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVector>
#include <QHeaderView>
#include <QMessageBox>
#include <QTimer>

#include <functional>
#include <QtCharts/QCategoryAxis>

#include "datameasurement.h"


// Главный класс для управления всем визуалом приложения
class DataVisualizer : public QObject
{
    Q_OBJECT
public:
    explicit DataVisualizer(QHBoxLayout* graphLayout1,
                            QHBoxLayout* graphLayout2,
                            QTableView* onlineTable,   // для онлайн-измерений (tableView)
                            QTableView* savedTable,    // для сохранённых (tableView_2)
                            QPushButton* startButton,
                            QPushButton* stopButton,
                            QPushButton* saveButton,
                            QPushButton* pauseButton,
                            QPushButton* autoButton,
                            QObject *parent = nullptr);

    // Обновление внешнего вида под разные состояния программы
    void setIdleView();
    void setMeasuringView();
    void setPausedView();
    void setAutoMeasuringView();

    // Добавить значение (вызывается только по кнопке "Сохранить")
    void addSavedValue(const QVector<MeasurementGroup>& groups);

    // Сбросить оба графика и таблицы
    void clearAll();

    QAbstractItemModel* savedModel() const;


public slots:
    // Слот для обновления онлайн-графика при изменении буфера
    void onBufferUpdated(const QVector<double>& values);
    void setSaveView(int seconds);

private:
    QGridLayout* m_layout;

    // Онлайн-график (график 1)
    QChart* m_rawChart;
    QLineSeries* m_rawSeries;
    QValueAxis* m_rawAxisX;
    QValueAxis* m_rawAxisY;
    QChartView* m_chartView;

    // График сохранённых значений (график 2)
    QChart* m_avgChart;
    QScatterSeries* m_firstPointSeries = nullptr;
    QLineSeries* m_avgSeries = nullptr;
    QCategoryAxis* m_avgAxisX;
    QValueAxis* m_avgAxisY;
    QChartView* m_chartView2;

    // Таблицы
    QStandardItemModel* m_onlineTableModel;
    QTableView* m_onlineTable;
    QStandardItemModel* m_savedTableModel;
    QTableView* m_savedTable;

    // Кнопки
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_saveButton;
    QPushButton* m_pauseButton;
    QPushButton* m_autoButton;

    QPushButton* m_newGroupControlButton = nullptr;  // Кнопка для добавления новой группы / сброса
    bool m_newGroupRequested = false;                // Флаг, активна ли заявка на новую группу


    //Окно сохранения значения
    QMessageBox* m_saveMsgBox = nullptr;
    QTimer* m_saveCountdownTimer = nullptr;
    int m_saveSecondsLeft;

    void drawTable(const QVector<MeasurementGroup>& groups);
    void drawGraph(const QVector<MeasurementGroup>& groups, std::function<double(const Measurement&)> valueAccessor);  // отрисовать весь график
    void resetAutoButton(); //сброс кнопки авторежима
signals:
    void saveTimeout();
};

#endif // DATAVISUALIZER_H

#include "datavisualizer.h"
#include <QStandardItem>
#include <QHeaderView>
#include <algorithm>

// Конструктор: инициализация всех визуальных компонентов
DataVisualizer::DataVisualizer(QHBoxLayout* graphLayout1,
                               QHBoxLayout* graphLayout2,
                               QTableView* onlineTable,
                               QTableView* savedTable,
                               QPushButton* startButton,
                               QPushButton* stopButton,
                               QPushButton* saveButton,
                               QPushButton* pauseButton,
                               QPushButton* autoButton,
                               QObject *parent)
    : QObject(parent),
    m_firstPointSeries(nullptr),
    m_onlineTable(onlineTable),
    m_savedTable(savedTable),
    m_startButton(startButton),
    m_stopButton(stopButton),
    m_saveButton(saveButton),
    m_pauseButton(pauseButton),
    m_autoButton(autoButton)
{
    // --- График 1 ---
    m_rawChart = new QChart();
    m_rawSeries = new QLineSeries();
    m_rawAxisX = new QValueAxis();
    m_rawAxisY = new QValueAxis();
    m_chartView = new QChartView(m_rawChart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    graphLayout1->addWidget(m_chartView);

    // --- График 2 ---
    m_avgChart = new QChart();
    m_avgAxisX = new QCategoryAxis();
    m_avgAxisY = new QValueAxis();
    m_chartView2 = new QChartView(m_avgChart);
    m_chartView2->setRenderHint(QPainter::Antialiasing);
    graphLayout2->addWidget(m_chartView2);

    // --- Таблицы ---
    m_onlineTableModel = new QStandardItemModel(this);
    m_onlineTableModel->setHorizontalHeaderLabels(QStringList() << "Расстояние без обработки");
    m_onlineTable->setModel(m_onlineTableModel);
    m_onlineTable->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_onlineTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_savedTableModel = new QStandardItemModel(this);
    m_savedTableModel->setHorizontalHeaderLabels(QStringList()
                                                 << "Расстояние"
                                                 << "Номер"
                                                 << "Ожидаемое"
                                                 << "Погрешность"
                                                 << "Режим");
    m_savedTable->setModel(m_savedTableModel);
    m_savedTable->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_savedTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Первичная инициализация отображения
    setIdleView();
}


// Онлайн-режим: обновление графика и онлайн-таблицы
void DataVisualizer::onBufferUpdated(const QVector<double>& values)
{
    m_rawSeries->clear();

    // Заполняем онлайн-таблицу и график
    m_onlineTableModel->removeRows(0, m_onlineTableModel->rowCount());
    for (int i = 0; i < values.size(); ++i) {
        double value = values[values.size() - 1 - i];
        m_onlineTableModel->insertRow(i, new QStandardItem(QString::number(value, 'f', 6)));
        m_rawSeries->append(i + 1, values[i]);
    }

    // Ось X: фиксированная (1..10)
    m_rawAxisX->setRange(1, 10);
    m_rawAxisX->setTickCount(10);
    m_rawAxisX->setLabelsVisible(true);
    m_rawAxisX->setLabelFormat("%d");

    // Ось Y: динамический диапазон по значениям буфера, метки включены
    if (!values.isEmpty()) {
        m_rawAxisY->setLabelsVisible(true);
        m_rawAxisY->setLabelFormat("%.6f");
        double minY = *std::min_element(values.begin(), values.end());
        double maxY = *std::max_element(values.begin(), values.end());
        if (minY == maxY) { minY -= 0.000001; maxY += 0.000001; }
        m_rawAxisY->setRange(minY, maxY);
        m_rawAxisY->setTickCount(10);
    } else {
        // Если пусто — не показываем деления
        m_rawAxisY->setLabelsVisible(false);
    }
}

// Добавить значение в таблицу и на график 2
void DataVisualizer::addSavedValue(const QVector<MeasurementGroup>& groups)
{
    drawTable(groups);
    drawGraph(groups, [](const Measurement& m) { return m.distance; });
}

void DataVisualizer::drawTable(const QVector<MeasurementGroup>& groups)
{
    m_savedTableModel->removeRows(0, m_savedTableModel->rowCount());

    for (const auto& group : groups) {
        // 1. Строка-заголовок группы
        QList<QStandardItem*> groupRow;
        auto* groupHeader = new QStandardItem(QString("Группа №%1").arg(group.groupId));
        groupHeader->setTextAlignment(Qt::AlignCenter);
        groupRow << groupHeader;
        m_savedTableModel->appendRow(groupRow);

        // Объединяем ячейку на всю ширину (5 колонок)
        m_savedTable->setSpan(m_savedTableModel->rowCount() - 1, 0, 1, 5);

        // ——— Стандартный вывод шагов
        for (const auto& series : group.steps) {
            QStringList distances, expecteds, deviations;
            bool hasExpected = (group.mode != StepMode::None);

            for (const auto& m : series.measurements) {
                distances  << QString::number(m.distance, 'f', 6);
                expecteds  << (hasExpected ? QString::number(m.expected, 'f', 6) : "-");
                deviations << (hasExpected ? QString::number(m.deviation, 'f', 6) : "-");
            }

            QList<QStandardItem*> row;
            auto* distItem = new QStandardItem(distances.join("\n"));
            auto* stepItem = new QStandardItem(QString::number(series.stepNumber));
            auto* expItem  = new QStandardItem(expecteds.join("\n"));
            auto* devItem  = new QStandardItem(deviations.join("\n"));

            // Настройка многострочного отображения
            distItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            distItem->setSizeHint(QSize(100, 20 * distances.size()));
            expItem->setSizeHint(QSize(100, 20 * expecteds.size()));
            devItem->setSizeHint(QSize(100, 20 * deviations.size()));

            row << distItem << stepItem << expItem << devItem;

            QString modeStr;
            switch (group.mode) {
                case StepMode::None:    modeStr = "Нет"; break;
                case StepMode::Uniform: modeStr = "Равномерный"; break;
                case StepMode::Manual:  modeStr = "Ручной"; break;
                case StepMode::Formula: modeStr = "Формула"; break;
            }

            row << new QStandardItem(modeStr);
            m_savedTableModel->appendRow(row);
        }
    }
}



void DataVisualizer::drawGraph(const QVector<MeasurementGroup>& groups,
                               std::function<double(const Measurement&)> valueAccessor)
{
    m_avgChart->removeAllSeries();
    m_avgSeries = nullptr;
    m_firstPointSeries = nullptr;

    delete m_avgAxisX;
    m_avgAxisX = new QCategoryAxis();
    m_avgAxisX->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    m_avgChart->addAxis(m_avgAxisX, Qt::AlignBottom);
    m_avgChart->addAxis(m_avgAxisY, Qt::AlignLeft);

    QVector<QPointF> points;
    QStringList labels;
    QSet<QString> seenLabels;

    int index = 1;
      for (const auto& group : groups) {
          for (const auto& series : group.steps) {
              for (const auto& m : series.measurements) {
                  double y = valueAccessor(m);
                  points.append(QPointF(index, y));

                  QString label = QString("%1.%2").arg(series.stepNumber).arg(m.repeatIndex);
                  while (seenLabels.contains(label)) {
                      label += QChar(0x200B);
                  }
                  seenLabels.insert(label);

                  m_avgAxisX->append(label, index);
                  ++index;
              }
          }
    }

    if (points.isEmpty())
        return;

    if (points.size() == 1) {
        m_firstPointSeries = new QScatterSeries();
        m_firstPointSeries->setMarkerSize(14.0);
        m_firstPointSeries->setColor(Qt::red);
        m_firstPointSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
        m_firstPointSeries->append(points[0]);

        m_avgChart->addSeries(m_firstPointSeries);
        m_firstPointSeries->attachAxis(m_avgAxisX);
        m_firstPointSeries->attachAxis(m_avgAxisY);

        m_avgAxisX->setRange(points[0].x() - 1, points[0].x() + 1);
        m_avgAxisX->setTickCount(3);
        m_avgAxisY->setRange(0, points[0].y() * 2.0);
        m_avgAxisY->setTickCount(3);
    } else {
        m_avgSeries = new QLineSeries();
        for (const auto& pt : points)
            m_avgSeries->append(pt);

        m_avgChart->addSeries(m_avgSeries);
        m_avgSeries->attachAxis(m_avgAxisX);
        m_avgSeries->attachAxis(m_avgAxisY);

        double minY = points[0].y(), maxY = points[0].y();
        for (const auto& pt : points) {
            if (pt.y() < minY) minY = pt.y();
            if (pt.y() > maxY) maxY = pt.y();
        }
        if (minY == maxY) { minY -= 0.000001; maxY += 0.000001; }

        m_avgAxisY->setRange(minY, maxY);
        m_avgAxisY->setTickCount(10);
    }

    m_avgAxisX->setLabelsVisible(true);
    m_avgAxisX->setLabelFormat("%s");  // для QCategoryAxis это игнорируется
    m_avgAxisY->setLabelFormat("%.6f");
    m_avgAxisY->setLabelsVisible(true);
}




void DataVisualizer::setSaveView(int seconds)
{
    m_saveSecondsLeft = seconds;

    // очистка предыдущего состояния
    if (m_saveMsgBox) {
        m_saveMsgBox->close();
        delete m_saveMsgBox;
        m_saveMsgBox = nullptr;
    }
    if (m_saveCountdownTimer) {
        m_saveCountdownTimer->stop();
        delete m_saveCountdownTimer;
        m_saveCountdownTimer = nullptr;
    }

    m_saveMsgBox = new QMessageBox();
    m_saveMsgBox->setWindowTitle("Подождите");
    m_saveMsgBox->setText(QString("Идёт измерение... Осталось %1 сек").arg(m_saveSecondsLeft));
    m_saveMsgBox->setStandardButtons(QMessageBox::Cancel);
    m_saveMsgBox->button(QMessageBox::Cancel)->hide();
    m_saveMsgBox->show();

    m_saveCountdownTimer = new QTimer(this);

    // 1. обработка закрытия по крестику
    connect(m_saveMsgBox, &QMessageBox::finished, this, [=](int){
        if (m_saveCountdownTimer) {
            m_saveCountdownTimer->stop();
            m_saveCountdownTimer->deleteLater();
            //delete m_saveCountdownTimer;
            m_saveCountdownTimer = nullptr;
        }

        if (m_saveMsgBox) {
            m_saveMsgBox->deleteLater();
            //delete m_saveMsgBox;
            m_saveMsgBox = nullptr;
        }

        emit saveTimeout(); // единый вызов
    });

    // 2. обработка по истечении времени
    connect(m_saveCountdownTimer, &QTimer::timeout, this, [=]() {
        m_saveSecondsLeft--;
        if (m_saveSecondsLeft > 0) {
            if (m_saveMsgBox)
                m_saveMsgBox->setText(QString("Идёт измерение... Осталось %1 сек").arg(m_saveSecondsLeft));
        } else {
            m_saveCountdownTimer->stop();

            // просто закрываем окно — `finished` сработает и сам вызовет saveTimeout
            if (m_saveMsgBox) {
                m_saveMsgBox->done(0);  // именно эта строка запустит finished()
            }
        }
    });

    m_saveCountdownTimer->start(1000);
}

// Сброс визуала для Idle-состояния
void DataVisualizer::setIdleView()
{
    // Кнопки
    m_startButton->setText("СТАРТ");
    m_startButton->setStyleSheet("");
    m_stopButton->setStyleSheet("");
    m_pauseButton->setText("ПАУЗА");
    m_saveButton->setStyleSheet("background-color: red; color: white;");

    // --- График 1 ---
    m_rawChart->addSeries(m_rawSeries);
    m_rawChart->addAxis(m_rawAxisX, Qt::AlignBottom);
    m_rawChart->addAxis(m_rawAxisY, Qt::AlignLeft);
    m_rawSeries->attachAxis(m_rawAxisX);
    m_rawSeries->attachAxis(m_rawAxisY);
    m_rawChart->setTitle("Онлайн-график расстояния");
    m_rawChart->legend()->hide();

    m_rawAxisX->setRange(1, 10);
    m_rawAxisX->setTickCount(10);
    m_rawAxisX->setLabelsVisible(true);
    m_rawAxisX->setLabelFormat("%d");

    m_rawAxisY->setLabelsVisible(false);

    // --- График 2 ---
    // Очистка и пересоздание категориальной оси X
    if (m_avgAxisX) {
        m_avgChart->removeAxis(m_avgAxisX);
        delete m_avgAxisX;
    }
    m_avgAxisX = new QCategoryAxis();
    m_avgAxisX->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);

    m_avgChart->addAxis(m_avgAxisX, Qt::AlignBottom);
    m_avgChart->addAxis(m_avgAxisY, Qt::AlignLeft);
    m_avgChart->setTitle("График сохранённых расстояний");
    m_avgChart->legend()->hide();

    // Фиктивная серия для отображения осей
    auto* dummySeries = new QLineSeries();
    dummySeries->setColor(Qt::transparent);
    m_avgChart->addSeries(dummySeries);
    dummySeries->attachAxis(m_avgAxisX);
    dummySeries->attachAxis(m_avgAxisY);

    // Заполняем ось X фиктивными значениями
    for (int i = 1; i <= 10; ++i)
        m_avgAxisX->append(" ", i);

    m_avgAxisY->setRange(0, 1);
    m_avgAxisX->setLabelsVisible(true);
    m_avgAxisY->setLabelsVisible(true);
    m_avgAxisX->setLabelFormat(" ");
    m_avgAxisY->setLabelFormat(" ");
    m_avgAxisX->setTickCount(10);
    m_avgAxisY->setTickCount(10);
    resetAutoButton();
}


// Состояние "Измерение": изменить стиль кнопки
void DataVisualizer::setMeasuringView()
{
    m_pauseButton->setText("ПАУЗА");
    m_startButton->setText("РЕСТАРТ");
    m_startButton->setStyleSheet("background-color: #4CAF50; color: white;");
    m_saveButton->setStyleSheet("background-color: #4CAF50; color: white;");
    resetAutoButton();
}


void DataVisualizer::setPausedView()
{
    // Меняем текст кнопки ПАУЗА на ПРОДОЛЖИТЬ
    m_pauseButton->setText("ПРОДОЛЖИТЬ");
    m_saveButton->setStyleSheet("background-color: red; color: white;");
    resetAutoButton();

}

// Сбросить графики, таблицы, внутренние структуры
void DataVisualizer::clearAll()
{
    m_avgChart->removeAllSeries();
    m_avgSeries = nullptr;
    m_firstPointSeries = nullptr;

    m_onlineTableModel->removeRows(0, m_onlineTableModel->rowCount());
    m_savedTableModel->removeRows(0, m_savedTableModel->rowCount());

    if (m_avgAxisX) {
        m_avgChart->removeAxis(m_avgAxisX);
        delete m_avgAxisX;
        m_avgAxisX = nullptr;
    }
}

QAbstractItemModel* DataVisualizer::savedModel() const {
    return m_savedTableModel;
}

// Состояние "Автосохранение": блокировка кнопок и смена стиля
void DataVisualizer::setAutoMeasuringView()
{
    // 1. Кнопка СТАРТ — неактивна
    m_startButton->setEnabled(false);

    // 2. Кнопка ПАУЗА — неактивна
    m_pauseButton->setEnabled(false);

    // 3. Кнопка СОХРАНИТЬ — неактивна
    m_saveButton->setEnabled(false);

    // 4. Кнопка АВТОСОХРАНЕНИЕ — активна и выделена
    if (m_autoButton) {
        m_autoButton->setEnabled(true);
        m_autoButton->setText("Остановить автосохранение");
        m_autoButton->setStyleSheet("background-color: red; color: white;");
    }
}

void DataVisualizer::resetAutoButton()
{
    if (!m_autoButton) return;
    m_autoButton->setEnabled(true);
    m_autoButton->setText("АВТОСОХРАНЕНИЕ");
    m_autoButton->setStyleSheet("");    // дефолтная серая
    m_startButton->setEnabled(true);
     m_pauseButton->setEnabled(true);
     m_saveButton->setEnabled(true);
}

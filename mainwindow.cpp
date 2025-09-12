#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "expectationfilter.h"
#include "averagefilter.h"
#include "stepconfigdialog.h"
#include "autoconfigdialog.h"
#include "nonefilter.h"

#include "accuracy/accuracywindow.h"

#include <QLabel>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QWidgetAction>

#include <QDebug>
#include <QTimer>



// Конструктор главного окна
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    fileManager(new FileManager(this))
{
    ui->setupUi(this);

    // Создаём все объекты ядра и визуализации
    appState   = new AppState(this);
    pyProc     = new PyProc(this);
    buffer     = new DataBuffer(this, 10);
    settingsManager = new SettingsManager(this);
    dataMeasurement = new DataMeasurement();
    autoSaver = new AutoMeasurement(buffer, dataMeasurement, this);


    // Добавляем GUI элементы, зависящие от settingsManager
    addTimeSetting();

    // при старте восстанавливаем состояние из settingsManager
    ui->actionBidirectional->setChecked(settingsManager->stepSettings().bidirectional);

    // Регистрируем фильтры
    settingsManager->registerFilter("Без фильтра", ui->actionNoneFilter, []() {
        return new NoneFilter();
    });
    settingsManager->registerFilter("Среднее арифметическое", ui->actionAverageFilter, []() {
        return new AverageFilter();
    });
    settingsManager->registerFilter("Фильтр с интерквартильным отклонением", ui->actionExpectationFilter, []() {
        return new ExpectationFilter();
    });

    // Создаём фильтр по умолчанию
    filter = settingsManager->createInitialFilter(this);

    // Подключаем реакцию на смену фильтра
    connect(settingsManager, &SettingsManager::filterChanged, this, [=](Filter* newFilter) {
        if (filter) {
            disconnect(buffer, &DataBuffer::updated, filter, &Filter::processData);
            delete filter;
        }
        filter = newFilter;
    });

    // Передаём все виджеты визуализатору
    visualizer = new DataVisualizer(
        ui->graphLayout1,
        ui->graphLayout2,
        ui->tableView,      // онлайн-таблица
        ui->tableView_2,    // сохранённые значения
        ui->start_button,
        ui->stop_button,
        ui->save_button,
        ui->pause_button,
        ui->auto_button,
        this
        );

    // Подключаем все сигналы/слоты (не относятся к настройкам)
    connect(appState, &AppState::stateChanged, this, &MainWindow::onAppStateChanged);

    connect(pyProc, &PyProc::distance, buffer, &DataBuffer::append);

    connect(buffer, &DataBuffer::updated, visualizer, &DataVisualizer::onBufferUpdated);

    //Отображение значения только после Save, переход из состояния в Save в другое
    connect(visualizer, &DataVisualizer::saveTimeout, this, &MainWindow::onValueReady);

    connect(pyProc, &PyProc::error, this, &MainWindow::onPyError);

    //Настройки шага и базовой точки
    connect(ui->actionStepSettings, &QAction::triggered, this, [=]() {
        // Если программа работает — сначала ставим на паузу
        if (appState->state() == ProgramState::Measuring) {
            appState->setState(ProgramState::Paused);
        }

        // Затем переходим в режим настройки шагов
        appState->setState(ProgramState::StepConfiguring);
    });

    //Настройки автосохранения
    connect(ui->actionAutoSave, &QAction::triggered, this, [=]() {
        if (appState->state() == ProgramState::Measuring) {
            appState->setState(ProgramState::Paused);
        }

        appState->setState(ProgramState::AutoConfiguring);
    });

    // После: autoSaver = new AutoMeasurement(buffer, dataMeasurement, this);
    connect(autoSaver, &AutoMeasurement::requestSaving,
            this, [=](){ appState->setState(ProgramState::Saving); });

    // Выходим из авто-режима по окончании плана
    connect(autoSaver, &AutoMeasurement::planFinished, this, [=](){
        appState->setState(ProgramState::Measuring);
    });

    // Кнопки в окне:
    //старт
    connect(ui->start_button, &QPushButton::clicked, this, [=]() {
        if (appState->state() == ProgramState::Measuring) {

            // "РЕСТАРТ" логика: сбросить всё и начать заново
            appState->setState(ProgramState::Stopped);

            // Важно: дожидаемся возврата в Idle, потом только запускаем
            // (это гарантируется в onAppStateChanged)
            QTimer::singleShot(0, this, [=]() {
                appState->setState(ProgramState::Measuring);
            });
        } else {
            // Обычный старт
            appState->setState(ProgramState::Measuring);
        }
    });

    //стоп
    connect(ui->stop_button, &QPushButton::clicked, this, [=](){
        appState->setState(ProgramState::Stopped);
    });

    //сохранить
    connect(ui->save_button, &QPushButton::clicked, this, [=]() {
        // Сохранять можно и в ручном, и в авто‑измерении
        const auto st = appState->state();
        if (st != ProgramState::Measuring && st != ProgramState::AutoMeasuring) {
            QMessageBox::warning(this, "Ошибка",
                                 "Запустите сбор данных, прежде чем сохранять.");
            return;
        }
        appState->setState(ProgramState::Saving); // единое событие сохранения
    });

    //пауза и продолжить
    connect(ui->pause_button, &QPushButton::clicked, this, [=](){
        if (appState->state() == ProgramState::Paused) {
            appState->setState(ProgramState::Measuring);  // продолжить
        } else if (appState->state() == ProgramState::Measuring) {
            appState->setState(ProgramState::Paused);     // поставить на паузу
        }
    });

    //кнопка рассчет
    connect(ui->cal_button, &QPushButton::clicked, this, [=]() {
        appState->setState(ProgramState::Processing);
    });

    //кнопка автосохранения
    connect(ui->auto_button, &QPushButton::clicked, this, [=]() {

        // Проверка допустимости включения
        if (appState->state() != ProgramState::Measuring &&
            appState->state() != ProgramState::AutoMeasuring)  // AutoSaving → AutoMeasuring после переименования
        {
            QMessageBox::warning(this, "Ошибка",
                                 "Авто-режим можно запустить только из состояния измерения.");
            return;
        }

        // Тумблер авто-режима
        if (appState->state() == ProgramState::AutoMeasuring) {
            appState->setState(ProgramState::Measuring);     // выход из авто-режима
        } else if (appState->state() == ProgramState::Measuring) {
            appState->setState(ProgramState::AutoMeasuring);    // вход в авто-режим
        }
    });

    // Изначально — состояние Idle
    appState->setState(ProgramState::Idle);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Обработка смены состояния приложения
void MainWindow::onAppStateChanged(ProgramState state)
{
    switch (state) {
    case ProgramState::Idle: {
        visualizer->setIdleView();
        buffer->clear();
        if (autoSaver && autoSaver->isRunning()) autoSaver->stop();

        // Установка актуальных настроек шагов
        // Инициализация базового плана по актуальным настройкам
        const StepSettings settings = settingsManager->stepSettings();
        planGen.makeBase(settings);

        // Сразу подготовим и применим текущий PlanSave в измерение
        if (auto saveHandle = planGen.makeSave()) {
            dataMeasurement->applyPlanSave(saveHandle);
        }
        break;
    }

    case ProgramState::Measuring: {
        if (!pyProc->isRunning())
            pyProc->start("C:/MY/Calibrix/scripts/parser_loop.py");
        visualizer->setMeasuringView();
        if (autoSaver && autoSaver->isRunning()) autoSaver->stop();
        break;
    }

    case ProgramState::Stopped: {
        pyProc->stop();              // остановить скрипт
        buffer->clear();             // очистить буфер онлайн-графика
        visualizer->clearAll();      // очистить графики и таблицы
        dataMeasurement->clear();    // очищаем все измерения
        visualizer->setIdleView();   // сброс визуала
        if (autoSaver && autoSaver->isRunning()) autoSaver->stop(); // сброс autoSaver
        appState->setState(ProgramState::Idle);  // возвращаемся в Idle

        break;
    }

    case ProgramState::Saving: {
        buffer->clear(); // очищаем буфер перед началом записи новых данных
        connect(buffer, &DataBuffer::updated, filter, &Filter::processData, Qt::UniqueConnection); //начата передача данных из буфера в фильтр
        visualizer->setSaveView(settingsManager->saveTime());         // показывает окно с обратным отсчётом
        break;
    }

    case ProgramState::Paused: {
        pyProc->stop();  // остановить скрипт
        visualizer->setPausedView(); // стиль кнопки и интерфейса
        break;
    }

    case ProgramState::StepConfiguring: {
        // 1) Создаём диалог и подгружаем сохранённые настройки
        stepconfigdialog dlg(this);
        dlg.loadFromSettings(settingsManager->stepSettings());

        // 2) Открываем модально — по btnSave вернётся QDialog::Accepted
        if (dlg.exec() == QDialog::Accepted) {

            // 3) Сохраняем в SettingsManager
            settingsManager->setStepSettings(dlg.currentSettings());

            // Пересобираем Base и выдаём новый Save в измерение
            planGen.makeBase(settingsManager->stepSettings());
            if (auto saveHandle = planGen.makeSave()) {
                dataMeasurement->applyPlanSave(saveHandle);
            }
            // Перестраиваем визуализацию под новые настройки
            visualizer->addSavedValue(dataMeasurement->groups());
        }

        // 3) Возвращаемся в предыдущие состояние
        if (appState->previousState() == ProgramState::Paused) {
            appState->setState(ProgramState::Measuring);
        } else {
            appState->setState(appState->previousState());
        }
        break;
    }

    case ProgramState::AutoConfiguring: {
        // 1. Открываем окно и подгружаем настройки
        autoconfigdialog dlg(this);
        dlg.loadFromSettings(settingsManager->autoSaveSettings());

        // 2. Пользователь нажал "Сохранить"
        if (dlg.exec() == QDialog::Accepted) {
            settingsManager->setAutoSaveSettings(dlg.currentSettings());
        }

        // 3. Возврат в предыдущее состояние
        if (appState->previousState() == ProgramState::Paused) {
            appState->setState(ProgramState::Measuring);
        } else {
            appState->setState(appState->previousState());
        }
        break;
    }

    case ProgramState::AutoMeasuring: {
        // 1. Включаем нужный интерфейс (отключаем все кнопки кроме auto/stop)
        visualizer->setAutoMeasuringView();

        // 2) Проверяем, что шаги заданы — без них авто‑режим невозможен
        if (settingsManager->stepSettings().mode == StepMode::None) {
            QMessageBox::warning(this, "Ошибка", "Авто‑режим невозможен: не заданы шаги.");
            appState->setState(ProgramState::Measuring);
            break;
        }

        // 3) Логика AutoSaver
        if (!autoSaver->isRunning()) {
            const StepSettings step = settingsManager->stepSettings();
            const AutoSaveSettings as = settingsManager->autoSaveSettings();
            const AutoSavePlan plan = autoSaver->createPlan(step, as, dataMeasurement);
            autoSaver->start(plan);
        }

        break;
    }

    case ProgramState::Processing: {
        double base = settingsManager->stepSettings().base;
        auto* accuracyWindow = new AccuracyWindow(this);
        accuracyWindow->setAttribute(Qt::WA_DeleteOnClose);  // автоудаление при закрытии
        accuracyWindow->setMeasurementData(*dataMeasurement, base);
        accuracyWindow->show();

        appState->setState(ProgramState::Paused);
        break;
    }
    case ProgramState::Error: {
        pyProc->stop();
        visualizer->setIdleView();
        break;
    }
    }
}

// Обработка готового усреднённого значения из фильтра
void MainWindow::onValueReady()
{
    disconnect(buffer, &DataBuffer::updated, filter, &Filter::processData);

    // Если фильтр не получил ни одного значения — значит за время измерения не было данных
    if (buffer->size() == 0) {
        QMessageBox::warning(this, "Нет данных",
                             "За время измерения не было получено новых значений.");
        filter->clear();
        const auto prev = appState->previousState();
        appState->setState(prev == ProgramState::AutoMeasuring
                               ? ProgramState::AutoMeasuring
                               : ProgramState::Measuring);
        return;
    }

    const double value = filter->result();
    dataMeasurement->add(value);
    filter->clear();
    visualizer->addSavedValue(dataMeasurement->groups());

    // Возвращаемся в исходное рабочее состояние
    if (appState->previousState() == ProgramState::AutoMeasuring && autoSaver->isRunning()) {
        autoSaver->savingFinished();
        appState->setState(ProgramState::AutoMeasuring);
    } else {
        appState->setState(ProgramState::Measuring);
    }
}

// Обработка ошибок запуска Python-процесса
void MainWindow::onPyError(const QString& msg)
{
    QMessageBox::critical(this, "Ошибка Python-процесса", msg);
    appState->setState(ProgramState::Error);
}

void MainWindow::on_actionSave_triggered()
{
    fileManager->exportToCsv(this, visualizer->savedModel());
}

void MainWindow::addTimeSetting()
{
    QWidget* timeWidget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(timeWidget);
    layout->setContentsMargins(10, 0, 10, 0);

    QLabel* label = new QLabel("Время измерения (сек):", timeWidget);
    QSpinBox* spinBox = new QSpinBox(timeWidget);
    spinBox->setRange(1, 999);
    spinBox->setValue(settingsManager->saveTime());

    layout->addWidget(label);
    layout->addWidget(spinBox);

    QWidgetAction* action = new QWidgetAction(this);
    action->setDefaultWidget(timeWidget);
    ui->menuSettings->insertAction(ui->menuSettings->actions().isEmpty() ? nullptr: ui->menuSettings->actions().first(), action);

    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), settingsManager, &SettingsManager::setSaveTime);
}



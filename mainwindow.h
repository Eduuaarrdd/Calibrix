#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include "appstate.h"
#include "pyproc.h"
#include "databuffer.h"
#include "filter.h"
#include "datavisualizer.h"
#include "filemanager.h"
#include "settingsmanager.h"
#include "datameasurement.h"
#include "automeasurement.h"
#include "plangenerator.h"



QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAppStateChanged(ProgramState state);
    void onValueReady();
    void onPyError(const QString& msg);
    void on_actionSave_triggered();

private:
    Ui::MainWindow *ui;
    AppState* appState;
    PyProc* pyProc;
    DataBuffer* buffer;
    Filter* filter = nullptr;
    DataVisualizer* visualizer;
    FileManager* fileManager;
    SettingsManager* settingsManager;
    DataMeasurement* dataMeasurement;
    AutoMeasurement* autoSaver = nullptr;
    void addTimeSetting();  // настройка строки времени измерения в меню
    PlanGenerator planGen;
};

#endif // MAINWINDOW_H




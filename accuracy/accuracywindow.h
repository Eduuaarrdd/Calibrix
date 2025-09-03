#ifndef ACCURACYWINDOW_H
#define ACCURACYWINDOW_H

#include <QMainWindow>
#include "datameasurement.h"
#include "accuracystate.h"
#include "exportmanager.h"

class AccuracyVisualizer;
namespace Ui { class AccuracyWindow; }

class AccuracyWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AccuracyWindow(QWidget* parent = nullptr);
    ~AccuracyWindow();

    void setMeasurementData(const DataMeasurement& copiedMeasurement, double basePoint);

private slots:
    void onDeleteRowClicked(int row);

    void onAddRowToGroup(int groupId);
    void onAddGroup();
    void onDeleteGroup(int groupId);

    void onSaveClicked();
    void onResetClicked();
    void onStateChanged(AccuracyWindowState state);

    void onCalculateButtonClicked();

    void onCellEdited(int row, int column);

    void onExportClicked();


private:
    Ui::AccuracyWindow* ui = nullptr;
    AccuracyState* state = nullptr;
    AccuracyVisualizer* visualizer = nullptr;

    DataMeasurement measurement;
    double basePoint = 0.0;

    ExportManager* exportManager = nullptr;
    DataMeasurement lastTempMeasurement;
    AccuracyResultList lastResults;
};

#endif // ACCURACYWINDOW_H



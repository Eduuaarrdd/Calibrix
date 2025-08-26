// settingsmanager.h
#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QMap>
#include <QAction>
#include <QActionGroup>
#include <functional>
#include <QString>
#include "filter.h"

// ——— Типы шагов ———
enum class StepMode {
    Formula,
    Manual,
    None,
    Uniform
};

// ——— Настройки шагов ———
struct StepSettings {
    StepMode mode = StepMode::None;
    double base = 0.0;

    double step = 1.0;
    int count = 10;

    QString manualText;
    QString formula;
    int formulaCount = 10;

    int repeatCount = 1;

    bool bidirectional = false;
};

// ——— Настройки автосохранения ———
struct AutoSaveSettings {
    bool autoGroup = false;          // radio_autoGroup
    double positiveTolerance = 0.002; // spin_positiveTolerance
    double negativeTolerance = 0.002; // spin_negativeTolerance
    double speedLimit = 0.01;         // spin_speedLimit
};

// ——— Менеджер ———
class SettingsManager : public QObject {
    Q_OBJECT

public:
    explicit SettingsManager(QObject* parent = nullptr);

    void registerFilter(const QString& name, QAction* action, std::function<Filter*()> factory);
    Filter* createInitialFilter(QObject* parent = nullptr) const;

    // ——— Геттер/сеттер настроек автосохранения ———
    void setStepSettings(const StepSettings& settings);
    StepSettings stepSettings() const;

    // ——— Геттер/сеттер настроек автосохранения ———
    void setAutoSaveSettings(const AutoSaveSettings& settings);
    AutoSaveSettings autoSaveSettings() const;

    int saveTime() const;
    void setSaveTime(int seconds);

signals:
    void filterChanged(Filter* newFilter);

private slots:
    void onFilterSelected(QAction* action);

private:
    QMap<QAction*, std::function<Filter*()>> m_filterFactories;
    QActionGroup* m_filterGroup;
    QAction* m_defaultAction = nullptr;

    StepSettings m_stepSettings;
    bool m_stepSettingsSet = false;
    bool m_autoRepeatEnabled = false;

    int m_saveTime = 5;

    AutoSaveSettings m_autoSaveSettings;
};

#endif // SETTINGSMANAGER_H



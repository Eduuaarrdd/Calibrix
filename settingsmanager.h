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
    StepMode mode = StepMode::Uniform;
    double base = 0.0;

    double step = 20.0;      // размер шага
    int count = 5;           // кол-во шагов

    QString manualText;
    QString formula;
    int formulaCount = 10;

    int repeatCount = 5;     // кол-во измерений

    bool bidirectional = true; // двунаправленное измерение
};

struct AutoSaveSettings {
    bool autoGroup = true;           // автосохранение всей группы шагов
    double positiveTolerance = 0.07; // верхний предел погрешности
    double negativeTolerance = 0.07; // нижний предел погрешности
    double speedLimit = 0.01;        // порог скорости
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

    int m_saveTime = 2;

    AutoSaveSettings m_autoSaveSettings;
};

#endif // SETTINGSMANAGER_H



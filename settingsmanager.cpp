#include "settingsmanager.h"
#include <QActionGroup>

SettingsManager::SettingsManager(QObject* parent)
    : QObject(parent),
    m_filterGroup(new QActionGroup(this))
{
    m_filterGroup->setExclusive(true);
    connect(m_filterGroup, &QActionGroup::triggered,
            this, &SettingsManager::onFilterSelected);
}

void SettingsManager::registerFilter(const QString& name,
                                     QAction* action,
                                     std::function<Filter*()> factory)
{
    m_filterFactories.insert(action, factory);
    action->setActionGroup(m_filterGroup);
    action->setText(name);
    if (!m_defaultAction) {
        m_defaultAction = action;
        action->setChecked(true);
    }
}

Filter* SettingsManager::createInitialFilter(QObject* parent) const
{
    if (m_defaultAction && m_filterFactories.contains(m_defaultAction)) {
        Filter* f = m_filterFactories.value(m_defaultAction)();
        if (parent) f->setParent(parent);
        return f;
    }
    return nullptr;
}

void SettingsManager::setStepSettings(const StepSettings& settings)
{
    m_stepSettings = settings;
    m_stepSettingsSet = true;
}

StepSettings SettingsManager::stepSettings() const
{
    return m_stepSettings;
}

void SettingsManager::onFilterSelected(QAction* action)
{
    if (m_filterFactories.contains(action)) {
        Filter* f = m_filterFactories.value(action)();
        emit filterChanged(f);
    }
}

int SettingsManager::saveTime() const {
    return m_saveTime;
}

void SettingsManager::setSaveTime(int seconds) {
    m_saveTime = seconds;
}

void SettingsManager::setAutoSaveSettings(const AutoSaveSettings& settings) {
    m_autoSaveSettings = settings;
}

AutoSaveSettings SettingsManager::autoSaveSettings() const {
    return m_autoSaveSettings;
}






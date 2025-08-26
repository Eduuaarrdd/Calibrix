#ifndef ACCURACYSTATE_H
#define ACCURACYSTATE_H

#include <QObject>

enum class AccuracyWindowState {
    Idle,
    Editing,
    Resetting,
    Saving,
    Calculating,
    Error
};

class AccuracyState : public QObject
{
    Q_OBJECT
public:
    explicit AccuracyState(QObject* parent = nullptr);

    AccuracyWindowState state() const { return m_state; }
    AccuracyWindowState previousState() const { return m_previousState; }

    void setState(AccuracyWindowState newState);

signals:
    void stateChanged(AccuracyWindowState newState);

private:
    AccuracyWindowState m_state = AccuracyWindowState::Idle;
    AccuracyWindowState m_previousState = AccuracyWindowState::Idle;
};

#endif // ACCURACYSTATE_H


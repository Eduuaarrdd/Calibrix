#include "accuracystate.h"

AccuracyState::AccuracyState(QObject* parent)
    : QObject(parent)
{
}

void AccuracyState::setState(AccuracyWindowState newState)
{
    m_previousState = m_state;
    m_state = newState;
    emit stateChanged(m_state);
}


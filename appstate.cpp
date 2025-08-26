#include "appstate.h"

AppState::AppState(QObject *parent)
    : QObject(parent)
    , m_state(ProgramState::Idle)
{
}

// Меняет состояние и посылает сигнал только если изменилось
void AppState::setState(ProgramState newState)
{
    if (m_state != newState) {
        m_previousState = m_state;     //сохраняем текущее перед сменой
        m_state = newState;
        emit stateChanged(m_state);
    }
}

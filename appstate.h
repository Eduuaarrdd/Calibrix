#ifndef APPSTATE_H
#define APPSTATE_H

#include <QObject>

// Ключевые состояния программы
enum class ProgramState {
    Idle,
    Measuring,
    AutoMeasuring,
    Processing,
    Stopped,
    Saving,
    Paused,
    StepConfiguring,
    AutoConfiguring,
    Error
};

class AppState : public QObject
{
    Q_OBJECT
public:
    explicit AppState(QObject *parent = nullptr);

    ProgramState state() const { return m_state; }

    // Основной метод смены состояния
    void setState(ProgramState newState);

    //Предыдущие состояние
    ProgramState previousState() const { return m_previousState; }


signals:
    void stateChanged(ProgramState newState);

private:
    ProgramState m_state = ProgramState::Idle;
    ProgramState m_previousState = ProgramState::Idle;
};

#endif // APPSTATE_H

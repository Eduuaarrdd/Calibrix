#ifndef STEPCONFIGDIALOG_H
#define STEPCONFIGDIALOG_H

#include <QDialog>
#include <QMap>
#include "settingsmanager.h"  // StepMode и StepSettings

namespace Ui {
class stepconfigdialog;
}

class stepconfigdialog : public QDialog
{
    Q_OBJECT

public:
    explicit stepconfigdialog(QWidget *parent = nullptr);
    ~stepconfigdialog();

    // Загрузить сохранённые настройки в UI
    void loadFromSettings(const StepSettings& settings);

    // Получить текущие настройки после редактирования
    StepSettings currentSettings() const;


private slots:
    // Переключение режимов
    void UniformToggled(bool checked);
    void ManualToggled(bool checked);
    void FormulaToggled(bool checked);
    void NoneToggled(bool checked);

    void Save();      // btnSave — сохраняет и закрывает диалог
    void Cancel();    // btnCancel — закрывает без сохранения
    void Default();   // btnDefault — сбрасывает формы к исходным значениям


private:
    // вспомогательный метод, который «одним махом» заполняет UI из любых переданных настроек
    void applyUI(const StepSettings& s);

    Ui::stepconfigdialog *ui;
    StepSettings m_settings;
    QMap<StepMode, QWidget*> m_modePages; // карта режим→страница QStackedWidget
};

#endif // STEPCONFIGDIALOG_H


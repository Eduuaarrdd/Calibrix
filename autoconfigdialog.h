#ifndef AUTOCONFIGDIALOG_H
#define AUTOCONFIGDIALOG_H

#include <QDialog>
#include "settingsmanager.h"

namespace Ui {
class autoconfigdialog;
}

class autoconfigdialog : public QDialog
{
    Q_OBJECT

public:
    explicit autoconfigdialog(QWidget *parent = nullptr);
    ~autoconfigdialog();

    void loadFromSettings(const AutoSaveSettings& settings);
    AutoSaveSettings currentSettings() const;

private slots:
    void Save();     // btn_save
    void Cancel();   // btn_cancel
    void Default();  // btn_default

private:
    void applyUI(const AutoSaveSettings& s);

    Ui::autoconfigdialog *ui;
    AutoSaveSettings settings;
};

#endif // AUTOCONFIGDIALOG_H

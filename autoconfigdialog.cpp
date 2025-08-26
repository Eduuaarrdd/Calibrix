#include "autoconfigdialog.h"
#include "ui_autoconfigdialog.h"

autoconfigdialog::autoconfigdialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::autoconfigdialog)
{
    ui->setupUi(this);
    setFixedSize(this->size());

    connect(ui->btn_save,    &QPushButton::clicked, this, &autoconfigdialog::Save);
    connect(ui->btn_cancel,  &QPushButton::clicked, this, &autoconfigdialog::Cancel);
    connect(ui->btn_default, &QPushButton::clicked, this, &autoconfigdialog::Default);
}

autoconfigdialog::~autoconfigdialog()
{
    delete ui;
}

void autoconfigdialog::loadFromSettings(const AutoSaveSettings& s)
{
    settings = s;
    applyUI(settings);
}

AutoSaveSettings autoconfigdialog::currentSettings() const
{
    return settings;
}

void autoconfigdialog::applyUI(const AutoSaveSettings& s)
{
    ui->radio_autoGroup->setChecked(s.autoGroup);
    ui->radio_autoMesurement->setChecked(!s.autoGroup);
    ui->spin_positiveTolerance->setValue(s.positiveTolerance);
    ui->spin_negativeTolerance->setValue(s.negativeTolerance);
    ui->spin_speedLimit->setValue(s.speedLimit);
}

void autoconfigdialog::Save()
{
    settings.autoGroup = ui->radio_autoGroup->isChecked();
    settings.positiveTolerance = ui->spin_positiveTolerance->value();
    settings.negativeTolerance = ui->spin_negativeTolerance->value();
    settings.speedLimit = ui->spin_speedLimit->value();

    accept();
}


void autoconfigdialog::Cancel()
{
    close(); // закрываем без сохранения
}

void autoconfigdialog::Default()
{
    settings = AutoSaveSettings(); // дефолтные
    applyUI(settings);
}

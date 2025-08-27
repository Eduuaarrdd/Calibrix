#include "stepconfigdialog.h"
#include "ui_stepconfigdialog.h"

stepconfigdialog::stepconfigdialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::stepconfigdialog)
{
    ui->setupUi(this);

    connect(ui->radioUniform, &QRadioButton::toggled, this, &stepconfigdialog::UniformToggled);
    connect(ui->radioManual,  &QRadioButton::toggled, this, &stepconfigdialog::ManualToggled);
    connect(ui->radioFormula, &QRadioButton::toggled, this, &stepconfigdialog::FormulaToggled);
    connect(ui->radioNone,    &QRadioButton::toggled, this, &stepconfigdialog::NoneToggled);

    //Подключение кнопки сохранить
    connect(ui->btnSave,   &QPushButton::clicked, this, &stepconfigdialog::Save);

    // кнопка «По умолчанию» — вернуть изначальные значения
    connect(ui->btnDefault,&QPushButton::clicked, this, &stepconfigdialog::Default);

    // карта страниц
    m_modePages[StepMode::Formula] = ui->pageFormula;
    m_modePages[StepMode::Manual]  = ui->pageManual;
    m_modePages[StepMode::None]    = ui->pageNone;
    m_modePages[StepMode::Uniform] = ui->pageUniform;

    // начальное состояние
    ui->radioNone->setChecked(true);
    NoneToggled(true);
}

stepconfigdialog::~stepconfigdialog()
{
    delete ui;
}

void stepconfigdialog::loadFromSettings(const StepSettings& settings)
{
    m_settings = settings;

    // 1) заполнить поля
    ui->spinBoxBase->setValue(m_settings.base);
    ui->doubleSpinBox_2->setValue(m_settings.step);
    ui->spinBox->setValue(m_settings.count);
    ui->plainTextEdit->setPlainText(m_settings.manualText);
    ui->lineEdit->setText(m_settings.formula);
    ui->spinBox_2->setValue(m_settings.formulaCount);
    ui->spinRepeatCount->setValue(m_settings.repeatCount);

    // 2) переключить страницу
    ui->stackedWidget->setCurrentWidget(m_modePages[m_settings.mode]);

    // 3) отразить режим в радиокнопках, не вызывая слотов
    ui->radioUniform->blockSignals(true);
    ui->radioManual->blockSignals(true);
    ui->radioFormula->blockSignals(true);
    ui->radioNone->blockSignals(true);

    switch (m_settings.mode) {
    case StepMode::Uniform: ui->radioUniform->setChecked(true); break;
    case StepMode::Manual:  ui->radioManual->setChecked(true);  break;
    case StepMode::Formula: ui->radioFormula->setChecked(true); break;
    case StepMode::None:    ui->radioNone->setChecked(true);    break;
    }

    ui->radioUniform->blockSignals(false);
    ui->radioManual->blockSignals(false);
    ui->radioFormula->blockSignals(false);
    ui->radioNone->blockSignals(false);

    ui->checkBidirectional->setChecked(m_settings.bidirectional);
}

StepSettings stepconfigdialog::currentSettings() const
{
    return m_settings;
}

void stepconfigdialog::UniformToggled(bool checked)
{
    if (checked) {
        m_settings.mode = StepMode::Uniform;
        ui->stackedWidget->setCurrentWidget(m_modePages[m_settings.mode]);
    }
}

void stepconfigdialog::ManualToggled(bool checked)
{
    if (checked) {
        m_settings.mode = StepMode::Manual;
        ui->stackedWidget->setCurrentWidget(m_modePages[m_settings.mode]);
    }
}

void stepconfigdialog::FormulaToggled(bool checked)
{
    if (checked) {
        m_settings.mode = StepMode::Formula;
        ui->stackedWidget->setCurrentWidget(m_modePages[m_settings.mode]);
    }
}

void stepconfigdialog::NoneToggled(bool checked)
{
    if (checked) {
        m_settings.mode = StepMode::None;
        ui->stackedWidget->setCurrentWidget(m_modePages[m_settings.mode]);
    }
}

void stepconfigdialog::Save()
{
    // считываем все поля один раз
    m_settings.base         = ui->spinBoxBase->value();
    m_settings.step         = ui->doubleSpinBox_2->value();
    m_settings.count        = ui->spinBox->value();
    m_settings.manualText   = ui->plainTextEdit->toPlainText();
    m_settings.formula      = ui->lineEdit->text();
    m_settings.formulaCount = ui->spinBox_2->value();
    m_settings.repeatCount = ui->spinRepeatCount->value();

    m_settings.bidirectional = ui->checkBidirectional->isChecked();

    accept();
}

// Сброс к фабричным настройкам по умолчанию
void stepconfigdialog::Default()
{
    m_settings = StepSettings();    // вызывает конструктор со "заводскими" значениями
    applyUI(m_settings);
}

// Выход без сохранения
void stepconfigdialog::Cancel()
{
    reject();
}

void stepconfigdialog::applyUI(const StepSettings& s)
{
    // 1) Обновляем все контролы значениями из s
    ui->spinBoxBase->setValue(s.base);
    ui->doubleSpinBox_2->setValue(s.step);
    ui->spinBox->setValue(s.count);
    ui->plainTextEdit->setPlainText(s.manualText);
    ui->lineEdit->setText(s.formula);
    ui->spinBox_2->setValue(s.formulaCount);
    ui->spinRepeatCount->setValue(s.repeatCount);

    // 2) Переключаем страницу
    ui->stackedWidget->setCurrentWidget(m_modePages[s.mode]);

    // 3) Обновляем радиокнопки без вызова слотов
    ui->radioUniform->blockSignals(true);
    ui->radioManual->blockSignals(true);
    ui->radioFormula->blockSignals(true);
    ui->radioNone->blockSignals(true);
    switch (s.mode) {
    case StepMode::Uniform: ui->radioUniform->setChecked(true); break;
    case StepMode::Manual:  ui->radioManual->setChecked(true);  break;
    case StepMode::Formula: ui->radioFormula->setChecked(true); break;
    case StepMode::None:    ui->radioNone->setChecked(true);    break;
    }
    ui->radioUniform->blockSignals(false);
    ui->radioManual->blockSignals(false);
    ui->radioFormula->blockSignals(false);
    ui->radioNone->blockSignals(false);

    ui->checkBidirectional->setChecked(s.bidirectional);
}






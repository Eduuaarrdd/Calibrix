#include "accuracyvisualizer.h"

#include <QLabel>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QCheckBox>
#include <QScrollBar>
#include <QMessageBox>

// ─────────────────────────────────────────────────────
// Конструктор и базовая инициализация
// ─────────────────────────────────────────────────────

AccuracyVisualizer::AccuracyVisualizer(QObject* parent)
    : QObject(parent)
{
}

// ─────────────────────────────────────────────────────
// Установка таблиц ввода и результатов для визуализатора
// ─────────────────────────────────────────────────────

void AccuracyVisualizer::setWidgets(QTableWidget* input, QTableWidget* result)
{
    inputTable = input;
    resultTable = result;

    // ───── Настройка таблицы ввода ─────
    if (inputTable) {
        inputTable->setColumnCount(7);
        inputTable->setHorizontalHeaderLabels(QStringList()
            << "Шаг.Повтор" << "Дистанция" << "Ожидаемое"
            << "Погрешность" << "Направление" << "Режим" << "Удалить");

        inputTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        connect(inputTable, &QTableWidget::currentCellChanged,
                this, &AccuracyVisualizer::onCurrentCellChanged);
    }

    // ───── Настройка таблицы результатов ─────
    resultTable->setColumnCount(12);
    QStringList labels;
    labels
        << "Позиция (шаг)"
        << "Номинал, мм"
        << "Откл. при подходе +, мм (x̄⁺)"
        << "Откл. при подходе −, мм (x̄⁻)"
        << "Среднее двунапр., мм (x̄ᵢ)"
        << "Коррекция (−x̄ᵢ), мм"
        << "Реверс, мм (Bᵢ)"
        << "Станд. отклонение при подходе +, мм (s⁺)"
        << "Станд. отклонение при подходе −, мм (s⁻)"
        << "Повторяемость при подходе +, мм (R⁺)"
        << "Повторяемость при подходе −, мм (R⁻)"
        << "Повторяемость двунаправленная, мм (Rᵢ)";
    resultTable->setHorizontalHeaderLabels(labels);

    // ───── Тултипы (формулы кратко) ─────
    auto setHdrTip = [&](int col, const QString& tip){
        auto* it = resultTable->horizontalHeaderItem(col);
        if (!it) { it = new QTableWidgetItem(labels[col]); resultTable->setHorizontalHeaderItem(col, it); }
        it->setToolTip(tip);
    };
    setHdrTip(2,  "x̄⁺ — среднее отклонение при подходе «+»");
    setHdrTip(3,  "x̄⁻ — среднее отклонение при подходе «−»");
    setHdrTip(4,  "x̄ᵢ = (x̄⁺ + x̄⁻)/2");
    setHdrTip(5,  "Рекомендуемая компенсация: Cᵢ = −x̄ᵢ");
    setHdrTip(6,  "Bᵢ = x̄⁺ − x̄⁻ — реверс (зона нечувств.)");
    setHdrTip(7,  "s⁺ — стандартное отклонение (подход «+»)");
    setHdrTip(8,  "s⁻ — стандартное отклонение (подход «−»)");
    setHdrTip(9,  "R⁺ = 4·s⁺");
    setHdrTip(10, "R⁻ = 4·s⁻");
    setHdrTip(11, "Rᵢ = max(2s⁺+2s⁻+|Bᵢ|, R⁺, R⁻)");
}



// ─────────────────────────────────────────────────────
// Сохранение видимой пользователем таблицы
// ─────────────────────────────────────────────────────
void AccuracyVisualizer::AccuracyVisualizer::saveVisibleTable()
{
    if (!inputTable)
        return;

    m_tableModel.clear();

    int rowCount = inputTable->rowCount();
    int colCount = inputTable->columnCount();

    int currentGroupId = -1;

    for (int row = 0; row < rowCount; ++row) {
        TableRow::Type type = TableRow::Type::Measurement;
        QWidget* widget = inputTable->cellWidget(row, 0);

        // 1. Определим тип строки

        // ───── Попытка: контейнер с QLabel → это заголовок группы ─────
        if (widget) {
            QLabel* label = widget->findChild<QLabel*>();
            if (label && label->text().startsWith("Группа №")) {
                type = TableRow::Type::GroupHeader;
            }
        }

        // ───── Кнопки ─────
        if (auto* btn = qobject_cast<QPushButton*>(widget)) {
            QString txt = btn->text();
            if (txt.contains("Добавить строку"))
                type = TableRow::Type::AddRowButton;
            else if (txt.contains("Добавить новую группу"))
                type = TableRow::Type::AddGroupButton;
        }

        // 2. Обработка по типу
        switch (type) {

        case TableRow::Type::GroupHeader: {
            TableRow header;
            header.type = type;

            QWidget* container = inputTable->cellWidget(row, 0);
            if (!container)
                break;

            // ——— Поиск QLabel и QCheckBox внутри контейнера ———
            QLabel* label = container->findChild<QLabel*>();
            QCheckBox* includeBox = container->findChild<QCheckBox*>("includeBox");

            // ——— ID группы из QLabel ———
            static const QRegularExpression re(R"(Группа №(\d+))");
            if (label) {
                QString text = label->text().trimmed();
                auto match = re.match(text);
                if (match.hasMatch()) {
                    header.groupId = match.captured(1).toInt();
                    currentGroupId = header.groupId;
                }
            }

            // ——— Выбранность чекбокса ———
            header.selectedFor = includeBox ? includeBox->isChecked() : true;

            // ——— Чекбокс "двунаправленный" справа ———
            if (auto* check = qobject_cast<QCheckBox*>(inputTable->cellWidget(row, colCount - 2))) {
                header.isBidirectional = check->isChecked();
            }

            m_tableModel.append(header);
            break;
        }

        case TableRow::Type::Measurement: {
            TableRow meas;
            meas.type = type;
            meas.groupId = currentGroupId;

            // шаг.пвтор
            auto* item0 = inputTable->item(row, 0);
            if (item0) {
                QStringList parts = item0->text().split(".");
                if (parts.size() == 2) {
                    meas.stepNumber = parts[0].toInt();
                    meas.repeatIndex = parts[1].toInt();
                }
            }

            // distance, expected, deviation
            auto* d = inputTable->item(row, 1);
            auto* e = inputTable->item(row, 2);
            auto* dev = inputTable->item(row, 3);
            QString distStr = d ? d->text().trimmed() : "";
            QString expectedStr = e ? e->text().trimmed() : "";
            QString deviationStr = dev ? dev->text().trimmed() : "";

            meas.distance = distStr.isEmpty() ? std::numeric_limits<double>::quiet_NaN() : distStr.toDouble();
            meas.expected = expectedStr.isEmpty() ? std::numeric_limits<double>::quiet_NaN() : expectedStr.toDouble();
            meas.deviation = deviationStr.isEmpty() ? std::numeric_limits<double>::quiet_NaN() : deviationStr.toDouble();

            // direction
            if (auto* dir = qobject_cast<QComboBox*>(inputTable->cellWidget(row, 4))) {
                meas.direction = directionFromString(dir->currentText());
            }

            // mode
            if (auto* mode = qobject_cast<QComboBox*>(inputTable->cellWidget(row, 5))) {
                meas.mode = modeFromString(mode->currentText());
            }

            m_tableModel.append(meas);
            break;
        }

        case TableRow::Type::AddRowButton: {
            TableRow rowBtn;
            rowBtn.type = type;
            rowBtn.groupId = currentGroupId;
            m_tableModel.append(rowBtn);
            break;
        }

        case TableRow::Type::AddGroupButton: {
            TableRow groupBtn;
            groupBtn.type = type;
            m_tableModel.append(groupBtn);
            break;
        }
        }
    }
}

// ─────────────────────────────────────────────────────
// Основной метод отображения начальных данных
// ─────────────────────────────────────────────────────

void AccuracyVisualizer::setTable(const DataMeasurement& measurement)
{
    if (!inputTable)
        return;

    // 0. Очистка визуального представления
    inputTable->clearContents();
    inputTable->setRowCount(0);

    int currentRow = 0;

    // 1. Проходим по всем группам
    for (const auto& group : measurement.groups()) {
        const int groupId = group.groupId;
        const bool isBidirectional = (group.type == MeasurementGroupType::Bidirectional);
        const bool selected = group.selectedFor;

        // 1.1 Заголовок группы
        inputTable->insertRow(currentRow);
        addGroupHeaderRow(currentRow, groupId, isBidirectional, selected);
        currentRow++;

        // 1.2 Измерения в группе
        for (const auto& step : group.steps) {
            for (const auto& m : step.measurements) {
                inputTable->insertRow(currentRow);
                addMeasurementRow(
                    currentRow,
                    step.stepNumber,
                    m.repeatIndex,
                    m.distance,
                    m.expected,
                    m.deviation,
                    step.direction,
                    group.mode
                );
                currentRow++;
            }
        }

        // 1.3 Кнопка "Добавить строку"
        inputTable->insertRow(currentRow);
        addAddRowButtonRow(currentRow, groupId);
        currentRow++;
    }

    // 2. Финальная кнопка "Добавить новую группу"
    inputTable->insertRow(currentRow);
    addGlobalAddGroupRow(currentRow);

    // 3. Сохраняем отображённую таблицу в модель
    saveVisibleTable();
}

// ─────────────────────────────────────────────────────
// Отображение строки с заголовком группы
// ─────────────────────────────────────────────────────

void AccuracyVisualizer::addGroupHeaderRow(int row, int groupId, bool isBidirectional, bool selected)
{
    const int colCount = inputTable->columnCount();

    // Объединяем ячейки под метку "Группа №N"
    inputTable->setSpan(row, 0, 1, colCount - 2);

    // ───── Метка + чекбокс "Участвует в расчёте" ─────
    auto* label = new QLabel(QString("Группа №%1").arg(groupId));
    label->setAlignment(Qt::AlignCenter);

    auto* includeBox = new QCheckBox("Включить группу в расчёт");
    includeBox->setObjectName("includeBox");
    includeBox->setChecked(selected);
    includeBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    includeBox->setToolTip("Включить группу в расчёт");

    // ——— Контейнер с горизонтальным выравниванием ———
    auto* container = new QWidget();
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->addWidget(includeBox, 0, Qt::AlignLeft);
    layout->addWidget(label, 1, Qt::AlignCenter);

    inputTable->setCellWidget(row, 0, container);

    // Чекбокс "двунаправленный"
    auto* check = new QCheckBox("Двунаправленный 🔁");
    check->setChecked(isBidirectional);
    check->setStyleSheet("margin-left:10px; margin-right:10px;");
    inputTable->setCellWidget(row, colCount - 2, check);

    // Кнопка "удалить"
    auto* deleteBtn = createDeleteButton();
    inputTable->setCellWidget(row, colCount - 1, deleteBtn);

    connect(deleteBtn, &QPushButton::clicked, this, [=]() {
        emit deleteGroupRequested(groupId);
    });
}


// ─────────────────────────────────────────────────────
// Отображение строки с измерением
// ─────────────────────────────────────────────────────

void AccuracyVisualizer::addMeasurementRow(int row,
                                           int stepNumber,
                                           int repeatIndex,
                                           double distance,
                                           double expected,
                                           double deviation,
                                           ApproachDirection direction,
                                           StepMode mode)
{
    // ───── 1. Шаг.Повтор ─────
    QString stepText = (stepNumber > 0 && repeatIndex > 0)
        ? QString("%1.%2").arg(stepNumber).arg(repeatIndex)
        : QString();  // Пустая ячейка
    auto* stepRepeatItem = new QTableWidgetItem(stepText);
    stepRepeatItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    inputTable->setItem(row, 0, stepRepeatItem);

    // ───── 2. Дистанция ─────
    QString distText = std::isnan(distance) ? "" : QString::number(distance, 'f', 6);
    auto* distanceItem = new QTableWidgetItem(distText);
    distanceItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    inputTable->setItem(row, 1, distanceItem);

    // ───── 3. Ожидаемое ─────
    QString expectedText = std::isnan(expected) ? "" : QString::number(expected, 'f', 6);
    auto* expectedItem = new QTableWidgetItem(expectedText);
    expectedItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    inputTable->setItem(row, 2, expectedItem);

    // ───── 4. Погрешность ─────
    QString deviationText = std::isnan(deviation) ? "" : QString::number(deviation, 'f', 6);
    auto* deviationItem = new QTableWidgetItem(deviationText);
    deviationItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    inputTable->setItem(row, 3, deviationItem);

    // ───── 5. Направление (ComboBox) ─────
    inputTable->setCellWidget(row, 4, createDirectionComboBox(direction));

    // ───── 6. Режим (ComboBox) ─────
    inputTable->setCellWidget(row, 5, createModeComboBox(mode));

    // ───── 7. Кнопка "удалить" ─────
    auto* delBtn = createDeleteButton();
    inputTable->setCellWidget(row, 6, delBtn);

    connect(delBtn, &QPushButton::clicked, this, [=]() {
        emit deleteRowRequested(row);  // передаём индекс строки напрямую
    });
}


// ─────────────────────────────────────────────────────
// Кнопка "Добавить строку" в группу
// ─────────────────────────────────────────────────────

void AccuracyVisualizer::addAddRowButtonRow(int row, int groupId)
{
    auto* btn = createAddRowButton();

    inputTable->setSpan(row, 0, 1, inputTable->columnCount());
    inputTable->setCellWidget(row, 0, btn);

    connect(btn, &QPushButton::clicked, this, [=]() {
        emit addRowToGroupRequested(groupId);
    });
}


// ─────────────────────────────────────────────────────
// Кнопка "Добавить группу"
// ─────────────────────────────────────────────────────

void AccuracyVisualizer::addGlobalAddGroupRow(int row)
{
    inputTable->setSpan(row, 0, 1, inputTable->columnCount());

    auto* btn = createAddGroupButton();
    inputTable->setCellWidget(row, 0, btn);

    connect(btn, &QPushButton::clicked, this, [=]() {
        emit addGroupRequested();
    });
}

// ─────────────────────────────────────────────────────
// Отображение таблицы по внутренней модели m_tableModel
// ─────────────────────────────────────────────────────
void AccuracyVisualizer::renderTable()
{
    if (!inputTable)
        return;

    // 0. Очистка текущей таблицы
    int scrollPos = inputTable->verticalScrollBar()->value();
    inputTable->clearContents();
    inputTable->setRowCount(0);

    int currentRow = 0;

    // 1. Отрисовываем строку за строкой
    for (const TableRow& row : m_tableModel) {
        inputTable->insertRow(currentRow);

        switch (row.type) {

        case TableRow::Type::GroupHeader:
            addGroupHeaderRow(currentRow, row.groupId, row.isBidirectional, row.selectedFor);
            break;

        case TableRow::Type::Measurement:
            addMeasurementRow(
                currentRow,
                row.stepNumber,
                row.repeatIndex,
                row.distance,
                row.expected,
                row.deviation,
                row.direction,
                row.mode
            );
            break;

        case TableRow::Type::AddRowButton:
            addAddRowButtonRow(currentRow, row.groupId);
            break;

        case TableRow::Type::AddGroupButton:
            addGlobalAddGroupRow(currentRow);
            break;
        }

        ++currentRow;
    }
    inputTable->verticalScrollBar()->setValue(scrollPos);
}

// ─────────────────────────────────────────────────────
// Компоненты UI
// ─────────────────────────────────────────────────────

QComboBox* AccuracyVisualizer::createDirectionComboBox(ApproachDirection dir)
{
    auto* combo = new QComboBox;
    combo->addItem("?", QVariant::fromValue(int(ApproachDirection::Unknown)));
    combo->addItem("Вперёд", QVariant::fromValue(int(ApproachDirection::Forward)));
    combo->addItem("Назад", QVariant::fromValue(int(ApproachDirection::Backward)));
    combo->setCurrentIndex(int(dir));
    return combo;
}

QComboBox* AccuracyVisualizer::createModeComboBox(StepMode mode)
{
    auto* combo = new QComboBox;
    combo->addItem("Нет", QVariant::fromValue(int(StepMode::None)));
    combo->addItem("Равномерный", QVariant::fromValue(int(StepMode::Uniform)));
    combo->addItem("Ручной", QVariant::fromValue(int(StepMode::Manual)));
    combo->addItem("Формула", QVariant::fromValue(int(StepMode::Formula)));

    combo->setCurrentIndex(combo->findData(QVariant::fromValue(int(mode))));  // ✅ фикс
    return combo;
}

QPushButton* AccuracyVisualizer::createDeleteButton()
{
    auto* btn = new QPushButton("✖");
    btn->setStyleSheet("color: red; font-weight: bold;");
    return btn;
}

QPushButton* AccuracyVisualizer::createAddRowButton()
{
    auto* btn = new QPushButton("➕ Добавить строку");
    btn->setStyleSheet("color: #007BFF; font-weight: bold;");
    return btn;
}

QPushButton* AccuracyVisualizer::createAddGroupButton()
{
    auto* btn = new QPushButton("➕ Добавить новую группу");
    btn->setStyleSheet("color: #007BFF; font-weight: bold;");
    return btn;
}

// ─────────────────────────────────────────────────────
// Добавление новой группы
// ─────────────────────────────────────────────────────
void AccuracyVisualizer::addGroup()
{
    if (!inputTable)
        return;

    // 1. Синхронизируем слепок модели с текущим отображением
    saveVisibleTable();

    // 2. Вычисляем ID новой группы (на 1 больше существующего максимума)
    int maxGroupId = 0;
    for (const auto& row : m_tableModel) {
        if ((row.type == TableRow::Type::GroupHeader || row.type == TableRow::Type::AddRowButton)
            && row.groupId > maxGroupId)
        {
            maxGroupId = row.groupId;
        }
    }
    int newGroupId = maxGroupId + 1;

    // 3. Готовим новые строки
    QVector<TableRow> newRows;

    // 3.1 Заголовок группы
    TableRow header;
    header.type = TableRow::Type::GroupHeader;
    header.groupId = newGroupId;
    header.isBidirectional = false;
    newRows.append(header);

    // 3.2 Кнопка "добавить строку"
    TableRow addRow;
    addRow.type = TableRow::Type::AddRowButton;
    addRow.groupId = newGroupId;
    newRows.append(addRow);

    // 4. Вставка перед "Добавить группу"
    int insertIndex = m_tableModel.size();
    for (int i = 0; i < m_tableModel.size(); ++i) {
        if (m_tableModel[i].type == TableRow::Type::AddGroupButton) {
            insertIndex = i;
            break;
        }
    }

    // Вставка по одному элементу, т.к. QVector не поддерживает insert с итераторами
    for (int i = 0; i < newRows.size(); ++i) {
        m_tableModel.insert(insertIndex + i, newRows[i]);
    }

    // 5. Перерисовываем таблицу по актуальной модели
    renderTable();
    saveVisibleTable();
}


// ─────────────────────────────────────────────────────
// Добавление новой строки измерения в указанную группу
// ─────────────────────────────────────────────────────
void AccuracyVisualizer::addRowToGroup(int groupId)
{
    if (!inputTable)
        return;

    // 1. Сохраняем текущее состояние таблицы в модель
    saveVisibleTable();

    // 2. Ищем:
    //    - последнюю строку измерения в группе
    //    - индекс строки "Добавить строку" в этой группе
    int insertIndex = -1;
    int lastStep = 0;
    bool foundMeasurement = false;
    bool isBidirectional = false;

    for (int i = 0; i < m_tableModel.size(); ++i) {
        const auto& row = m_tableModel[i];

        // Найдём флаг направления
        if (row.type == TableRow::Type::GroupHeader && row.groupId == groupId) {
            isBidirectional = row.isBidirectional;
        }

        // Последняя строка измерения
        if (row.type == TableRow::Type::Measurement && row.groupId == groupId) {
            insertIndex = i + 1;
            lastStep = row.stepNumber;
            foundMeasurement = true;
        }

        // Если ещё не найдено ни одной строки измерения — вставим перед AddRowButton
        if (!foundMeasurement && row.type == TableRow::Type::AddRowButton && row.groupId == groupId) {
            insertIndex = i;
        }
    }

    if (insertIndex == -1)
        return;  // группа повреждена — выходим

    // 3. Создаём новую строку измерения
    TableRow newRow;
    newRow.type = TableRow::Type::Measurement;
    newRow.groupId = groupId;

    if (isBidirectional) {
        newRow.stepNumber = 0; //
        newRow.repeatIndex = 0; //
    } else {
        newRow.stepNumber = foundMeasurement ? lastStep + 1 : 1;
        newRow.repeatIndex = 1;
    }

    newRow.distance = std::numeric_limits<double>::quiet_NaN();    // пусто
    newRow.expected = std::numeric_limits<double>::quiet_NaN();    // пусто
    newRow.deviation = std::numeric_limits<double>::quiet_NaN();   // пусто
    newRow.direction = ApproachDirection::Unknown;
    newRow.mode = StepMode::None;

    // 4. Вставляем новую строку перед кнопкой
    m_tableModel.insert(insertIndex, newRow);

    // 5. Перерисовываем таблицу и фиксируем новое состояние
    renderTable();
    saveVisibleTable();
}


// ─────────────────────────────────────────────────────
// Удаление всей группы по ID
// ─────────────────────────────────────────────────────
void AccuracyVisualizer::deleteGroup(int groupId)
{
    if (!inputTable)
        return;

    // 1. Сохраняем текущее состояние таблицы
    saveVisibleTable();

    // 2. Удаляем все строки с нужным groupId
    for (int i = m_tableModel.size() - 1; i >= 0; --i) {
        const auto& row = m_tableModel[i];
        if ((row.type == TableRow::Type::GroupHeader ||
             row.type == TableRow::Type::Measurement ||
             row.type == TableRow::Type::AddRowButton) &&
            row.groupId == groupId)
        {
            m_tableModel.removeAt(i);
        }
    }

    // 3. Перенумеровываем остальные группы (только если нужно)
    int nextGroupId = 1;
    QMap<int, int> groupIdMap;  // старый → новый

    for (auto& row : m_tableModel) {
        if (row.type == TableRow::Type::GroupHeader) {
            if (row.groupId != nextGroupId) {
                groupIdMap[row.groupId] = nextGroupId;
                row.groupId = nextGroupId;
            } else {
                groupIdMap[row.groupId] = row.groupId;
            }
            ++nextGroupId;
        } else if (row.groupId > 0 && groupIdMap.contains(row.groupId)) {
            row.groupId = groupIdMap[row.groupId];
        }
    }

    // 4. Перерисовываем таблицу
    renderTable();

    // 5. Сохраняем модель
    saveVisibleTable();
}


// ─────────────────────────────────────────────────────
// Удаление строки измерения по группе, шагу и повтору
// ─────────────────────────────────────────────────────
void AccuracyVisualizer::deleteRow(int row)
{
    if (!inputTable)
        return;

    // 1. Сохраняем текущее состояние UI
    saveVisibleTable();

    // 2. Ищем и удаляем строку с заданным шагом и повтором в нужной группе

    m_tableModel.removeAt(row);


    // 3. Перерисовываем таблицу и обновляем модель
    renderTable();
    saveVisibleTable();
}

// ─────────────────────────────────────────────────────
// Преобразование строки в enum направления
// ─────────────────────────────────────────────────────
ApproachDirection AccuracyVisualizer::directionFromString(const QString& str) const
{
    if (str == "Вперёд")
        return ApproachDirection::Forward;
    if (str == "Назад")
        return ApproachDirection::Backward;
    return ApproachDirection::Unknown;
}

// ─────────────────────────────────────────────────────
// Преобразование строки в enum режима
// ─────────────────────────────────────────────────────
StepMode AccuracyVisualizer::modeFromString(const QString& str) const
{
    if (str == "Равномерный")
        return StepMode::Uniform;
    if (str == "Ручной")
        return StepMode::Manual;
    if (str == "Формула")
        return StepMode::Formula;
    return StepMode::None;
}

// ─────────────────────────────────────────────────────
// Подготовка слепка таблицы: сохраняем модель + отображаем её
// ─────────────────────────────────────────────────────
QVector<TableRow> AccuracyVisualizer::prepareSnapshot()
{
    saveVisibleTable();   // 1. Сохраняем UI в модель
    renderTable();        // 2. Отображаем как есть (что реально сохранится)
    return m_tableModel;  // 3. Возвращаем КОПИЮ для внешнего кода
}

// ─────────────────────────────────────────────────────
// Отображение таблицы результатов расчёта точности
// ─────────────────────────────────────────────────────

void AccuracyVisualizer::setResultTable(const AccuracyResultList& results)
{
    if (!resultTable)
        return;

    // ───── Подготовка таблицы ─────
    resultTable->clearContents();
    resultTable->setRowCount(0);

    int currentGroup = 1;

    for (int i = 0; i < results.size(); ++i) {
        const AccuracyResult& r = results[i];

        // ───── Заголовок группы ─────
        if (r.stepNumber != -1 && (i == 0 || results[i - 1].stepNumber == -1)) {
            resultTable->insertRow(resultTable->rowCount());

            auto* headerItem = new QTableWidgetItem(
                QString("Расчёт для группы №%1").arg(currentGroup++));
            headerItem->setTextAlignment(Qt::AlignCenter);
            headerItem->setBackground(Qt::lightGray);
            QFont font = headerItem->font();
            font.setBold(true);
            headerItem->setFont(font);

            resultTable->setSpan(resultTable->rowCount() - 1, 0, 1, resultTable->columnCount());
            resultTable->setItem(resultTable->rowCount() - 1, 0, headerItem);
        }

        // ───── Строка с заголовками итогов группы ─────
        if (r.stepNumber == -1) {
            resultTable->insertRow(resultTable->rowCount());
            QStringList groupSummaryLabels = {
                " ", " ",
                "Диапазон средних (E), мм",
                "Макс. реверс (Bmax), мм",
                "Повторяемость по оси (R), мм",
                "Двунапр. погрешность (A), мм"
            };

            for (int col = 0; col < groupSummaryLabels.size(); ++col) {
                auto* item = new QTableWidgetItem(groupSummaryLabels[col]);
                item->setTextAlignment(Qt::AlignCenter);
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
                item->setBackground(QColor(235, 240, 255));
                resultTable->setItem(resultTable->rowCount() - 1, col, item);
            }
        }

        // ───── Строка с результатами ─────
        resultTable->insertRow(resultTable->rowCount());
        int row = resultTable->rowCount() - 1;

        // 1. Шаг (обычный или метка итогов)
        QString stepText = (r.stepNumber == -1)
            ? "Итог по группе"
            : QString::number(r.stepNumber);
        resultTable->setItem(row, 0, createReadonlyItem(stepText));

        // 2. Ожидаемое значение (если есть)
        QString expText = qIsNaN(r.expectedPosition)
            ? QString("—")
            : QString::number(r.expectedPosition, 'f', 6);
        resultTable->setItem(row, 1, createReadonlyItem(expText));

        // 3. Показатели по шагам (если stepNumber != -1)
        if (r.stepNumber != -1) {
            resultTable->setItem(row, 2,  createReadonlyItem(r.meanForward));         // x̄⁺
            resultTable->setItem(row, 3,  createReadonlyItem(r.meanBackward));        // x̄⁻
            resultTable->setItem(row, 4,  createReadonlyItem(r.meanBidirectional));   // x̄ᵢ
            resultTable->setItem(row, 5,  createReadonlyItem(-r.meanBidirectional));  // Коррекция (−x̄ᵢ) ← новая
            resultTable->setItem(row, 6,  createReadonlyItem(r.reversalError));       // Bᵢ
            resultTable->setItem(row, 7,  createReadonlyItem(r.stddevForward));       // s⁺
            resultTable->setItem(row, 8,  createReadonlyItem(r.stddevBackward));      // s⁻
            resultTable->setItem(row, 9,  createReadonlyItem(r.repeatabilityForward));    // R⁺
            resultTable->setItem(row, 10, createReadonlyItem(r.repeatabilityBackward));   // R⁻
            resultTable->setItem(row, 11, createReadonlyItem(r.repeatabilityBidirectional)); // Rᵢ
        }

        // 4. Итоги группы (если stepNumber == -1)
        else {
            resultTable->setItem(row, 0, createReadonlyItem("Итог по группе"));
            resultTable->setItem(row, 1, createReadonlyItem("—"));
            resultTable->setItem(row, 2, createReadonlyItem(r.meanRange));              // Eᵢ
            resultTable->setItem(row, 3, createReadonlyItem(r.systematicError));        // M
            resultTable->setItem(row, 4, createReadonlyItem(r.repeatabilityBidirectional)); // Rmax
            resultTable->setItem(row, 5, createReadonlyItem(r.positioningAccuracy));    // A
        }
    }

    // ───── Подгонка ширины столбцов ─────
    resultTable->resizeColumnsToContents();
}



// ─────────────────────────────────────────────────────
// Обработка редактирования ячеек "Шаг", "Дистанция" и "Ожидаемое"
// ─────────────────────────────────────────────────────

void AccuracyVisualizer::onCellEdited(int row, int column)
{
    if (!inputTable)
        return;

    QTableWidgetItem* editedItem = inputTable->item(row, column);
    if (!editedItem)
        return;

    QString text = editedItem->text().trimmed();

    // ───── Проверка колонки 0: формат шага должен быть вида "1.1" ─────
    if (column == 0) {
        if (text.isEmpty())
            return;  // пустая ячейка допускается

        // Формат: обязательно одна точка, обе части — положительные числа > 0
        QStringList parts = text.split('.');

        bool part1Ok = false;
        bool part2Ok = false;

        double part1 = parts.value(0).toDouble(&part1Ok);
        double part2 = parts.value(1).toDouble(&part2Ok);

        bool valid = (parts.size() == 2 &&
                      part1Ok && part2Ok &&
                      part1 > 0.0 && part2 > 0.0 &&
                      text.count('.') == 1);

        if (!valid) {
            QMessageBox::warning(inputTable, "Ошибка формата",
                                 "Формат шага должен быть вида 1.1 (оба числа положительные, разделены точкой)");

            inputTable->blockSignals(true);
            editedItem->setText(previousCellText);
            inputTable->blockSignals(false);
        }

        return;
    }


    // ───── Обрабатываем только колонки: 1 = Дистанция, 2 = Ожидаемое ─────
    if (column != 1 && column != 2)
        return;

    // ───── Проверка: введено ли число ─────
    bool ok = false;
    text.toDouble(&ok);

    if (!ok && !text.isEmpty()) {
        QMessageBox::warning(inputTable, "Ошибка ввода",
                             "Введите корректные числовые данные (разделитель — точка).");

        inputTable->blockSignals(true);
        editedItem->setText(previousCellText);
        inputTable->blockSignals(false);
        return;
    }

    // ───── Получаем ячейки для расчёта погрешности ─────
    QTableWidgetItem* distItem = inputTable->item(row, 1);  // Дистанция
    QTableWidgetItem* expItem  = inputTable->item(row, 2);  // Ожидаемое
    QTableWidgetItem* devItem  = inputTable->item(row, 3);  // Погрешность

    if (!distItem || !expItem || !devItem)
        return;

    QString distText = distItem->text().trimmed();
    QString expText  = expItem->text().trimmed();

    bool distOk = false;
    bool expOk  = false;

    double distVal = distText.toDouble(&distOk);
    double expVal  = expText.toDouble(&expOk);

    // ───── Если одно из значений отсутствует — очищаем ячейку "Погрешность" ─────
    if (distText.isEmpty() || expText.isEmpty()) {
        inputTable->blockSignals(true);
        devItem->setText("");
        inputTable->blockSignals(false);
        return;
    }

    // ───── Оба значения корректны — считаем погрешность ─────
    if (distOk && expOk) {
        double deviation = distVal - expVal;

        inputTable->blockSignals(true);
        devItem->setText(QString::number(deviation, 'f', 6));
        inputTable->blockSignals(false);
    }
}




// ─────────────────────────────────────────────────────
// Сохраняем текст текущей ячейки перед редактированием
// ─────────────────────────────────────────────────────
void AccuracyVisualizer::onCurrentCellChanged(int currentRow, int currentCol, int, int)
{
    if (!inputTable)
        return;

    QTableWidgetItem* item = inputTable->item(currentRow, currentCol);
    previousCellText = item ? item->text() : "";
}


QTableWidgetItem* AccuracyVisualizer::createReadonlyItem(double value)
{
    auto* item = new QTableWidgetItem(QString::number(value, 'f', 6));
    item->setFlags(item->flags() ^ Qt::ItemIsEditable);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QTableWidgetItem* AccuracyVisualizer::createReadonlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() ^ Qt::ItemIsEditable);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

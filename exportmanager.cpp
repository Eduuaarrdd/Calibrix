#include "exportmanager.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

ExportManager::ExportManager(QObject* parent) : QObject(parent) {}

// простая экранизация CSV
void ExportManager::writeCsvLine(QTextStream& out, const QStringList& cols, QChar sep)
{
    QStringList safe; safe.reserve(cols.size());
    for (QString s : cols) {
        if (s.contains('"')) s.replace("\"", "\"\"");
        if (s.contains(sep) || s.contains('\n') || s.contains('"')) s = "\"" + s + "\"";
        safe << s;
    }
    out << safe.join(sep) << "\n";
}

QString ExportManager::f6(double v)
{
    return qIsNaN(v) ? QString() : QString::number(v, 'f', 6);
}

QString ExportManager::dirToStr(ApproachDirection d)
{
    switch (d) {
    case ApproachDirection::Forward:  return "Вперёд";
    case ApproachDirection::Backward: return "Назад";
    default:                          return "?";
    }
}

QString ExportManager::modeToStr(StepMode m)
{
    switch (m) {
    case StepMode::Uniform: return "Равномерный";
    case StepMode::Manual:  return "Ручной";
    case StepMode::Formula: return "Формула";
    default:                return "Нет";
    }
}

bool ExportManager::exportAccuracyCsv(QWidget* parent,
                                      const DataMeasurement& measurement,
                                      const AccuracyResultList& results)
{
    if (measurement.groups().isEmpty() && results.isEmpty()) {
        QMessageBox::warning(parent, "Экспорт", "Нет данных для экспорта.");
        emit exportFailure("empty");
        return false;
    }

    const QString path = QFileDialog::getSaveFileName(
        parent, "Сохранить как CSV", "", "CSV файлы (*.csv)");
    if (path.isEmpty()) return false;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(parent, "Экспорт", "Не удалось открыть файл для записи.");
        emit exportFailure("open failed");
        return false;
    }

    QTextStream out(&f);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    out.setEncoding(QStringConverter::Utf8);
#endif
    out << QChar(0xFEFF); // BOM для Excel

    // ───────────────────────────────────────────────────────────
    // СЕКЦИЯ 1. Таблица ввода (максимально похоже на UI)
    // ───────────────────────────────────────────────────────────
    // В UI заголовок: "Шаг.Повтор", "Дистанция", "Ожидаемое", "Погрешность", "Направление", "Режим", "Удалить"
    // В CSV кнопку "Удалить" не дублируем.
    out << "Таблица ввода\n";
    writeCsvLine(out, {"Шаг.Повтор","Дистанция","Ожидаемое","Погрешность","Направление","Режим"});

    for (const auto& g : measurement.groups()) {
        // В UI заголовок группы — отдельная объединённая строка + справа чекбоксы.
        // В CSV сделаем служебную строку-группу:
        const bool isBi = (g.type == MeasurementGroupType::Bidirectional);
        writeCsvLine(out, {
                              QString("Группа №%1").arg(g.groupId),
                              QString(), QString(), QString(),
                              QString("Двунаправленный: %1").arg(isBi ? "Да" : "Нет"),
                              QString("Включить в расчёт: %1").arg(g.selectedFor ? "Да" : "Нет")
                          });

        // Строки измерений — как на экране
        for (const auto& s : g.steps) {
            for (const auto& m : s.measurements) {
                const QString stepRepeat =
                    (s.stepNumber > 0 && m.repeatIndex > 0)
                        ? QString("%1.%2").arg(s.stepNumber).arg(m.repeatIndex)
                        : QString();
                writeCsvLine(out, {
                                      stepRepeat,
                                      f6(m.distance),
                                      f6(m.expected),
                                      f6(m.deviation),
                                      dirToStr(s.direction),
                                      modeToStr(g.mode)
                                  });
            }
        }

        // В UI после группы идёт строка "➕ Добавить строку" — для CSV не нужна.
    }

    out << "\n";

    // ───────────────────────────────────────────────────────────
    // СЕКЦИЯ 2. Таблица результатов (как у AccuracyVisualizer::setResultTable)
    // ───────────────────────────────────────────────────────────
    out << "Таблица результатов\n";
    // Заголовок колонок как в визуализаторе (12 штук):
    writeCsvLine(out, {
                          "Позиция (шаг)",
                          "Номинал, мм",
                          "Откл. при подходе +, мм (x̄⁺)",
                          "Откл. при подходе −, мм (x̄⁻)",
                          "Среднее двунапр., мм (x̄ᵢ)",
                          "Коррекция (−x̄ᵢ), мм",
                          "Реверс, мм (Bᵢ)",
                          "Станд. отклонение при подходе +, мм (s⁺)",
                          "Станд. отклонение при подходе −, мм (s⁻)",
                          "Повторяемость при подходе +, мм (R⁺)",
                          "Повторяемость при подходе −, мм (R⁻)",
                          "Повторяемость двунаправленная, мм (Rᵢ)"
                      });

    // Чтобы сохранить группировку как в UI, вставим строку "Расчёт для группы №N"
    // каждый раз после "итога по группе" (stepNumber == -1) начинается следующая группа.
    int currentGroup = 1;
    bool needHeaderForCurrentGroup = true;

    for (int i = 0; i < results.size(); ++i) {
        const auto& r = results[i];

        if (needHeaderForCurrentGroup && r.stepNumber != -1) {
            // В UI вставляется объединённая строка заголовка группы
            writeCsvLine(out, {QString("Расчёт для группы №%1").arg(currentGroup)});
            needHeaderForCurrentGroup = false;
        }

        if (r.stepNumber != -1) {
            // Обычная строка результата по шагу
            writeCsvLine(out, {
                                  QString::number(r.stepNumber),
                                  f6(r.expectedPosition),
                                  f6(r.meanForward),
                                  f6(r.meanBackward),
                                  f6(r.meanBidirectional),
                                  f6(-r.meanBidirectional),
                                  f6(r.reversalError),
                                  f6(r.stddevForward),
                                  f6(r.stddevBackward),
                                  f6(r.repeatabilityForward),
                                  f6(r.repeatabilityBackward),
                                  f6(r.repeatabilityBidirectional)
                              });
        } else {
            // Итог по группе. В UI после этой строки дальше идёт следующая группа.
            // Визуализатор рисует отдельную "строку заголовков итогов", но фактически полезные значения в "Итог по группе".
            writeCsvLine(out, {
                                  "Итог по группе",
                                  "—",
                                  "Диапазон средних (E), мм",
                                  "Макс. реверс (Bmax), мм",
                                  "Повторяемость по оси (R), мм",
                                  "Двунапр. погрешность (A), мм",
                                  f6(r.meanRange),             // E
                                  f6(r.systematicError),       // M = Bmax
                                  f6(r.repeatabilityBidirectional), // Rmax
                                  f6(r.positioningAccuracy),   // A
                                  "", "" // выравнивание до 12 колонок
                              });

            // Подготовка к следующей группе
            currentGroup++;
            needHeaderForCurrentGroup = true;
        }
    }

    f.close();
    emit exportSuccess(path);
    QMessageBox::information(parent, "Экспорт", "CSV успешно сохранён:\n" + path);
    return true;
}


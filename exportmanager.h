#ifndef EXPORTMANAGER_H
#define EXPORTMANAGER_H

#include <QObject>
#include <QString>
#include "datameasurement.h"
#include "accuracy/accuracycalculator.h"

// Экспорт в CSV двух таблиц в формате, максимально похожем на визуализацию:
// 1) "Таблица ввода" — как в AccuracyVisualizer: групповые заголовки + строки измерений
// 2) "Таблица результатов" — как в AccuracyVisualizer::setResultTable (шаги + итог по группе)
//
// ВАЖНО: Класс НИЧЕГО НЕ СЧИТАЕТ. На вход подаём уже готовые tempMeasurement и results.

class QWidget;

class ExportManager : public QObject
{
    Q_OBJECT
public:
    explicit ExportManager(QObject* parent = nullptr);

    // Откроет диалог «Сохранить как…» и запишет один CSV с двумя секциями.
    bool exportAccuracyCsv(QWidget* parent,
                           const DataMeasurement& measurement,
                           const AccuracyResultList& results);

signals:
    void exportSuccess(const QString& path);
    void exportFailure(const QString& error);

private:
    static void writeCsvLine(QTextStream& out, const QStringList& cols, QChar sep = ';');
    static QString f6(double v);               // форматирование 6 знаков или ""
    static QString dirToStr(ApproachDirection d);
    static QString modeToStr(StepMode m);
};

#endif // EXPORTMANAGER_H



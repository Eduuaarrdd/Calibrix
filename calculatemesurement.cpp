#include "calculatemesurement.h"

#include <QStringList>
#include <QRegularExpression>
#include <cmath>

// ------------------------
// БЛОК 1. БАЗОВЫЕ РАСЧЁТЫ
// ------------------------

double CalculateMesurement::distance(double raw, double base)
{
    return raw - base;
}

double CalculateMesurement::deviation(double d, double e)
{
    if (std::isnan(e)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return d - e;
}

double CalculateMesurement::expectedUniform(int stepNumber, double step, double base)
{
    if (stepNumber <= 0) return 0.0;            // защита от нулей/отрицательных
    return base+stepNumber * step;
}

double CalculateMesurement::expectedManual(int stepNumber, const QString& text, double base)
{
    const QVector<double> list = parseManual(text);  // вариант A: парсим на каждый вызов
    return expectedManualFromList(stepNumber, list, base);
}

double CalculateMesurement::expectedManualFromList(int stepNumber, const QVector<double>& list, double base)
{
    if (stepNumber <= 0) return 0.0;
    if (stepNumber > list.size()) return 0.0;       // выход за границы — 0.0 по договорённости
    return base+list[stepNumber - 1];
}

double CalculateMesurement::expectedFormula(int /*stepNumber*/, const QString& /*formula*/, int /*n*/, double /*base*/)
{
    // Осознанная заглушка — формулы пока не поддерживаются
    return 0.0;
}

// ------------------------
// БЛОК 2. ПАРСЕР MANUAL
// ------------------------

QVector<double> CalculateMesurement::parseManual(const QString& text)
{
    if (text.isEmpty()) return {};

    static const QRegularExpression sepRe(R"([,;\s]+)"); // разделители: запятая/точка с запятой/пробелы
    const QStringList toks = text.split(sepRe, Qt::SkipEmptyParts);

    QVector<double> out;
    out.reserve(toks.size());

    for (const QString& t : toks) {
        bool ok = false;
        const double v = t.toDouble(&ok);
        if (ok) out.push_back(v);              // токены, не распарсившиеся в число, пропускаем
    }
    return out;
}

int CalculateMesurement::manualCount(const QString& text)
{
    return parseManual(text).size();
}


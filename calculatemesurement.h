#ifndef CALCULATEMESUREMENT_H
#define CALCULATEMESUREMENT_H

#include <QVector>
#include <QString>
#include <limits>

/*
 * Независимый калькулятор.
 * Никаких знаний о складе/настройках — только "чистая математика".
 */
class CalculateMesurement
{
public:

    // ------------------------
    // БЛОК 1. БАЗОВЫЕ РАСЧЁТЫ
    // ------------------------

    // Смещение относительно базы
    static double distance(double raw, double base);

    // Отклонение: distance - expected
    static double deviation(double distance, double expected);

    // Частные реализации "ожидаемого"
    static double expectedUniform(int stepNumber, double step, double base);
    static double expectedManual (int stepNumber, const QString& text, double base);
    static double expectedManualFromList(int stepNumber, const QVector<double>& list, double base);
    static double expectedFormula(int stepNumber, const QString& formula, int n, double base); // заглушка

    // ------------------------
    // БЛОК 2. ПАРСЕР MANUAL
    // ------------------------

    // Разобрать строку Manual в список чисел
    static QVector<double> parseManual(const QString& manualText);

    // Количество элементов в Manual-строке
    static int             manualCount(const QString& manualText);
};

#endif // CALCULATEMESUREMENT_H

#ifndef FILTER_H
#define FILTER_H

#include <QObject>
#include <QVector>

class Filter : public QObject
{
    Q_OBJECT
public:
    explicit Filter(QObject *parent = nullptr);
    virtual ~Filter() = default;

    virtual void clear();   // очистка буфера (вместо start)
    virtual double result(); // возвращает результат (вместо stop/compute)
    virtual void processData(const QVector<double>& values);

protected:
    QVector<double> m_buffer;
    double m_lastResult = 0.0;

    virtual double compute() = 0; // потомки реализуют вычисление
};

#endif // FILTER_H




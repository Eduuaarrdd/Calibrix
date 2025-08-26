#ifndef DATABUFFER_H
#define DATABUFFER_H

#include <QObject>
#include <QVector>

class DataBuffer : public QObject
{
    Q_OBJECT
public:
    explicit DataBuffer(QObject *parent = nullptr, int capacity = 10);

    void append(double value);          // Добавить новое измерение
    void clear();                       // Очистить буфер
    QVector<double> values() const;     // Получить текущие значения буфера
    int size() const;                   // Количество элементов в буфере
    int capacity() const;               // Максимальная вместимость

signals:
    void updated(const QVector<double>& values); // Сигнал: буфер обновлён

private:
    QVector<double> m_buffer;
    int m_capacity;
};

#endif // DATABUFFER_H

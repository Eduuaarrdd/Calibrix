#include "databuffer.h"

DataBuffer::DataBuffer(QObject *parent, int capacity)
    : QObject(parent), m_capacity(capacity)
{
    m_buffer.reserve(m_capacity);
}

void DataBuffer::append(double value)
{
    if (m_buffer.size() < m_capacity) {
        m_buffer.append(value*1000.000000);
    } else {
        // Сдвигаем влево и добавляем новое значение в конец
        for (int i = 0; i < m_capacity - 1; ++i)
            m_buffer[i] = m_buffer[i + 1];
        m_buffer[m_capacity - 1] = value*1000.000000;
    }
    emit updated(m_buffer);
}

void DataBuffer::clear()
{
    m_buffer.clear();
    emit updated(m_buffer);
}

QVector<double> DataBuffer::values() const
{
    return m_buffer;
}

int DataBuffer::size() const
{
    return m_buffer.size();
}

int DataBuffer::capacity() const
{
    return m_capacity;
}

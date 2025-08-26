#include "filter.h"

Filter::Filter(QObject *parent)
    : QObject(parent)
{
}

void Filter::clear()
{
    m_buffer.clear();
    m_lastResult = 0.0;
}

double Filter::result()
{
    m_lastResult = compute();
    return m_lastResult;
}

void Filter::processData(const QVector<double>& values)
{
    m_buffer += values;
}



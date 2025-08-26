#include "averagefilter.h"
#include <numeric>

AverageFilter::AverageFilter(QObject* parent)
    : Filter(parent)
{
}

double AverageFilter::compute()
{
    if (m_buffer.isEmpty()) {
        return 0.0;
    }

    double sum = std::accumulate(m_buffer.begin(), m_buffer.end(), 0.0);
    return sum / m_buffer.size();
}


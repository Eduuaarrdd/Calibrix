#include "expectationfilter.h"
#include <algorithm>
#include <numeric>

ExpectationFilter::ExpectationFilter(QObject* parent)
    : Filter(parent)
{
}

double ExpectationFilter::compute()
{
    if (m_buffer.isEmpty()) {
        return 0.0;
    }

    QVector<double> sorted = m_buffer;
    std::sort(sorted.begin(), sorted.end());

    int n = sorted.size();
    double q1 = sorted[n / 4];
    double q3 = sorted[3 * n / 4];
    double iqr = q3 - q1;

    double lower = q1 - 1.5 * iqr;
    double upper = q3 + 1.5 * iqr;

    QVector<double> filtered;
    for (double val : sorted) {
        if (val >= lower && val <= upper)
            filtered.append(val);
    }

    if (filtered.isEmpty()) {
        return q1;
    }

    double sum = std::accumulate(filtered.begin(), filtered.end(), 0.0);
    return sum / filtered.size();
}




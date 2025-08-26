#ifndef AVERAGEFILTER_H
#define AVERAGEFILTER_H

#include "filter.h"

class AverageFilter : public Filter
{
    Q_OBJECT
public:
    explicit AverageFilter(QObject* parent = nullptr);

protected:
    double compute() override;
};

#endif // AVERAGEFILTER_H

#ifndef EXPECTATIONFILTER_H
#define EXPECTATIONFILTER_H

#include "filter.h"

class ExpectationFilter : public Filter
{
    Q_OBJECT

public:
    explicit ExpectationFilter(QObject* parent = nullptr);

protected:
    double compute() override;
};

#endif // EXPECTATIONFILTER_H


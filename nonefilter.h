#ifndef NONEFILTER_H
#define NONEFILTER_H

#include "filter.h"

class NoneFilter : public Filter
{
    Q_OBJECT

public:
    explicit NoneFilter(QObject* parent = nullptr);

protected:
    double compute() override;
};

#endif // NONEFILTER_H


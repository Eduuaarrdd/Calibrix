#include "nonefilter.h"
#include <cstdlib>
#include <ctime>

NoneFilter::NoneFilter(QObject* parent)
    : Filter(parent)
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

double NoneFilter::compute()
{
    if (m_buffer.isEmpty())
        return 0.0;

    int index = std::rand() % m_buffer.size();
    return m_buffer[index];
}

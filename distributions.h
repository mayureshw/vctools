#ifndef _DISTRIBUTIONS_H
#define _DISTRIBUTIONS_H

#include <random>

class IDistribution
{
protected:
    random_device _rng;
public:
    virtual unsigned long gen()=0;
    virtual ~IDistribution() {}
};

template <typename D> class Distribution : public IDistribution
{
    D _distr;
public:
    unsigned long gen()
    {
        unsigned long delay = _distr(_rng);
        cout << "delay=" << delay << endl;
        return delay;
    }
    Distribution(double Avg) : _distr(Avg) {}
    ~Distribution() {}
};

#endif

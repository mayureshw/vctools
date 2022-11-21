#ifndef _DISTRIBUTIONS_H
#define _DISTRIBUTIONS_H

#include <random>

#include <functional>
#include <string>
#include <map>
#include <iostream>
using namespace std;
#include "xsb2cpp.h"

class IDistribution
{
protected:
    random_device _rng;
public:
    virtual double gen()=0;
    virtual ~IDistribution() {}
};

template <typename D, typename... Targs> class Distribution : public IDistribution
{
    D _distr;
public:
    double gen() { return _distr(_rng); }
    Distribution(Targs... args) : _distr(args...) {}
    ~Distribution() {}
};

// Please refer to the distributions here
// https://en.cppreference.com/w/cpp/numeric/random
// For constructor arguments, click on linke "(constructor)" on the distribution's page

#define CREATOR(DISTNAME,ARITY) { #DISTNAME, { ARITY, &DistFactory::create_##DISTNAME } }
#define CREATE1(DISTNAME,P1) \
    static IDistribution* create_##DISTNAME(PTerm* t) \
    { \
        double P1 = getarg<float>(t, 0); \
        return new Distribution< DISTNAME##_distribution<double>, double>(P1); \
    }
#define CREATE2(DISTNAME,P1,P2) \
    static IDistribution* create_##DISTNAME(PTerm* t) \
    { \
        double P1 = getarg<float>(t, 0), P2 = getarg<float>(t, 1); \
        return new Distribution< DISTNAME##_distribution<double>, double, double>(P1,P2); \
    }

class DistFactory
{
    template<typename T> static T getarg(PTerm *t, int pos)
    {
        return PDb::atom2val<T>(t->args()[pos]);
    }
    CREATE2 ( uniform_real,  a,      b      )
    CREATE1 ( exponential,   lambda         )
    CREATE2 ( gamma,         alpha,  beta   )
    CREATE2 ( weibull,       a,      b      )
    CREATE2 ( extreme_value, a,      b      )
    CREATE2 ( normal,        mean,   stddev )
    CREATE2 ( lognormal,     m,      s      )
    CREATE1 ( chi_squared,   n              )
    CREATE2 ( cauchy,        a,      b      )
    CREATE2 ( fisher_f,      m,      n      )
    CREATE1 ( student_t,     n              )
    map< string, pair<int, function<IDistribution*(PTerm*)> >> _distcreators =
    {
        CREATOR ( uniform_real,  2 ),
        CREATOR ( exponential,   1 ),
        CREATOR ( gamma,         2 ),
        CREATOR ( weibull,       2 ),
        CREATOR ( extreme_value, 2 ),
        CREATOR ( normal,        2 ),
        CREATOR ( lognormal,     2 ),
        CREATOR ( chi_squared,   1 ),
        CREATOR ( cauchy,        2 ),
        CREATOR ( fisher_f,      2 ),
        CREATOR ( student_t,     1 ),
    };
public:
    IDistribution* get(PTerm* t)
    {
        auto distname = t->functor();
        auto it = _distcreators.find( distname );
        if ( it == _distcreators.end() )
        {
            cout << "distfactory: No creator for distribution " << distname << endl;
            exit(1);
        }
        int exparity = it->second.first;
        int tarity = t->arity();
        if ( exparity != tarity )
        {
            cout << "Invalid arity for term " << t->tostr()
                << " expected: " << exparity << " got " << tarity << endl;
            exit(1);
        }
        return it->second.second(t);
    };
};

#endif

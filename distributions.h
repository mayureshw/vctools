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
    double gen()
    {
        double delay = _distr(_rng);
        cout << "delay=" << delay << endl;
        return delay;
    }
    Distribution(Targs... args) : _distr(args...) {}
    ~Distribution() {}
};

// Please refer to the distributions here
// https://en.cppreference.com/w/cpp/numeric/random
// For constructor arguments, click on linke "(constructor)" on the distribution's page

#define CREATOR(DISTNAME,ARITY) { #DISTNAME, { ARITY, &DistFactory::create_##DISTNAME } }
class DistFactory
{
    template<typename T> static T getarg(PTerm *t, int pos)
    {
        return PDb::atom2val<T>(t->args()[pos]);
    }
    static IDistribution* create_uniform(PTerm* t)
    {
        double a = getarg<float>(t, 0), b = getarg<float>(t, 1);
        return new Distribution< uniform_real_distribution<double>, double, double>(a,b);
    }
    static IDistribution* create_exponential(PTerm* t)
    {
        double lambda = getarg<float>(t, 0);
        return new Distribution< exponential_distribution<double>, double>(lambda);
    }
    static IDistribution* create_gamma(PTerm* t)
    {
        double alpha = getarg<float>(t, 0), beta = getarg<float>(t, 1);
        return new Distribution< gamma_distribution<double>, double, double>(alpha,beta);
    }
    static IDistribution* create_weibull(PTerm* t)
    {
        double a = getarg<float>(t, 0), b = getarg<float>(t, 1);
        return new Distribution< weibull_distribution<double>, double, double>(a,b);
    }
    static IDistribution* create_extreme_value(PTerm* t)
    {
        double a = getarg<float>(t, 0), b = getarg<float>(t, 1);
        return new Distribution< extreme_value_distribution<double>, double, double>(a,b);
    }
    static IDistribution* create_normal(PTerm* t)
    {
        double mean = getarg<float>(t, 0), stddev = getarg<float>(t, 1);
        return new Distribution< normal_distribution<double>, double, double>(mean,stddev);
    }
    static IDistribution* create_lognormal(PTerm* t)
    {
        double m = getarg<float>(t, 0), s = getarg<float>(t, 1);
        return new Distribution< lognormal_distribution<double>, double, double>(m,s);
    }
    static IDistribution* create_chi_squared(PTerm* t)
    {
        double n = getarg<float>(t, 0);
        return new Distribution< chi_squared_distribution<double>, double>(n);
    }
    static IDistribution* create_cauchy(PTerm* t)
    {
        double a = getarg<float>(t, 0), b = getarg<float>(t, 1);
        return new Distribution< cauchy_distribution<double>, double, double>(a,b);
    }
    static IDistribution* create_fisher_f(PTerm* t)
    {
        double m = getarg<float>(t, 0), n = getarg<float>(t, 1);
        return new Distribution< fisher_f_distribution<double>, double, double>(m,n);
    }
    static IDistribution* create_student_t(PTerm* t)
    {
        double n = getarg<float>(t, 0);
        return new Distribution< student_t_distribution<double>, double>(n);
    }
    map< string, pair<int, function<IDistribution*(PTerm*)> >> _distcreators =
    {
        CREATOR(uniform,      2),
        CREATOR(exponential,  1),
        CREATOR(gamma,        2),
        CREATOR(weibull,      2),
        CREATOR(extreme_value,2),
        CREATOR(normal,       2),
        CREATOR(lognormal,    2),
        CREATOR(chi_squared,  1),
        CREATOR(cauchy,       2),
        CREATOR(fisher_f,     2),
        CREATOR(student_t,    1),
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

#ifndef _VCPETRINET_H
#define _VCPETRINET_H

#ifdef USECEP
#   include "cpp2xsb.h"
#   define PN_USE_EVENT_LISTENER
#endif

#include "petrinet.h"

// Class VcPetriNet does the following:
//
// With USECEP: It provides a place to hold vctid relation to build the Prolog
// export of data required for the CEP functionality.
//
// With USESTPN: It provides a function to set the delay model in transitions
// using the specs in delaymodel.P

#ifdef USESTPN
#include <random>
#include "xsb2cpp.h"
#include "distributions.h"
using PetriNetVariant = STPetriNet;
#else
using PetriNetVariant = MTPetriNet;
#endif

using namespace std;

class VcPetriNet : public PetriNetVariant
{
using PetriNetVariant::PetriNetVariant;
public:
#ifdef USECEP
    Rel<unsigned,string,unsigned> vctid = {"vctid"};
#endif

#ifdef USESTPN
private:
    const t_predspec p_maxavg = {"maxavg",1}, p_delaymodel = {"delaymodel",3};
    PDb _delaydb;
    map<int, IDistribution*> _distmap;
    double _scalefactor;
    void setScalefactor()
    {
        auto maxavgs = _delaydb.get(p_maxavg);
        if ( maxavgs.size() != 1 )
        {
            cout << "maxavg predicate result size expected to be == 1, found " << maxavgs.size() << endl;
            exit(1);
        }
        auto maxavgterm = maxavgs.front()->args()[0];
        int maxavg = _delaydb.term2val<int>( maxavgterm );
        _scalefactor = (((unsigned long)-1)>>1) / maxavg;
    }
    void setdistmap()
    {
        list<tuple<int,string,int>> tl;
        _delaydb.terms2tuples(p_delaymodel, tl);
        for( auto dt : tl )
        {
            auto tid = get<0>(dt);
            auto dist = get<1>(dt);
            auto avg = get<2>(dt);
            double scaledavg = avg * _scalefactor;
            IDistribution *idist;
            if ( dist == "uniform" )
                idist = new Distribution<uniform_int_distribution<unsigned long>>(scaledavg);
            else if ( dist == "poisson" )
                idist = new Distribution<poisson_distribution<unsigned long>>(scaledavg);
            else
            {
                cout << "Unknown distribution: " << dist << endl;
                exit(1);
            }
            _distmap.emplace(tid, idist);
        }
    }
    void loadDelayModel()
    {
        _delaydb.call( "delaymodel" , { p_maxavg, p_delaymodel } );
        setScalefactor();
        setdistmap();
    }
public:
    void setDelayModels()
    {
        loadDelayModel();
        for(auto t:_transitions)
        {
            auto it = _distmap.find(t->_nodeid);
            if ( it != _distmap.end() )
                t->setDelayFn( bind(&IDistribution::gen,it->second) );
        }
    }

    ~VcPetriNet()
    {
        for(auto dt:_distmap) delete dt.second;
    }

#endif
};

#endif

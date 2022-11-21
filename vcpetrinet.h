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
    DistFactory _df;
    PTerm *_defaultModelTerm;
    const t_predspec p_defaultmodel = {"defaultmodel",1}, p_delaymodel = {"delaymodel",2};
    PDb _delaydb;
    map<int, IDistribution*> _distmap;
    void setdistmap()
    {
        auto dms = _delaydb.get( p_defaultmodel );
        if ( dms.size() != 1 )
        {
            cout << "defaultmodel/1 must be specified" << endl;
            exit(1);
        }
        for( auto t : _delaydb.get( p_defaultmodel ) )
        {
            _defaultModelTerm = t->args()[0];
            break;
        }
        for( auto t : _delaydb.get( p_delaymodel ) )
        {
            auto tid = t->args()[0]->asInt();
            auto dist = t->args()[1];
            auto idist = _df.get(dist);
            _distmap.emplace(tid, idist);
        }
    }
    void loadDelayModel()
    {
        _delaydb.call( "delaymodel" , { p_defaultmodel, p_delaymodel } );
        setdistmap();
    }
public:
    void setDelayModels()
    {
        loadDelayModel();
        for(auto t:_transitions)
        {
            auto it = _distmap.find( t->_nodeid );
            auto idist = it != _distmap.end() ? it->second :
                _df.get( _defaultModelTerm );
            t->setDelayFn( bind(&IDistribution::gen,idist) );
        }
    }

    ~VcPetriNet()
    {
        for(auto dt:_distmap) delete dt.second;
    }

#endif
};

#endif

#ifndef _VCPETRINET_H
#define _VCPETRINET_H

#ifdef USECEP
#   include "cpp2xsb.h"
#   define PN_USE_EVENT_LISTENER
#endif

#include "petrinet.h"

// Note: USECEP is also asserted by the Makefile when any of the randomized
// simulation modes is active.
#ifdef USECEP
    using PetriNetVariant = STPetriNet;
#else
    using PetriNetVariant = MTPetriNet;
#endif

// Class VcPetriNet does the following:
//
// With USECEP: It provides a place to hold vctid relation to build the Prolog
// export of data required for the CEP functionality.
//
// With SIMU_MODE_STPN: It provides a function to set the delay model in
// transitions using the specs in delaymodel.P

#ifdef SIMU_MODE_STPN
#include <random>
#include "xsb2cpp.h"
#include "distributions.h"
#endif

using namespace std;

typedef enum {
    Mutex_,
    PassiveBranch_,
    Branch_,
    SimuOnly_,
    } PNAnnotation;

class VcPetriNet : public PetriNetVariant
{
using PetriNetVariant::PetriNetVariant;

    map<PNAnnotation,set<PNNode*>> _annotations = {
        { Mutex_, {} },
        { PassiveBranch_, {} },
        { Branch_, {} },
        { SimuOnly_, {} },
        };
public:
#ifdef USECEP
    Rel<unsigned,string,unsigned> vctid = {"vctid"};
#endif
    set<PNNode*>& getAnnotatedNodeset(PNAnnotation annotation)
    {
        auto it = _annotations.find(annotation);
        if ( it == _annotations.end() )
        {
            cout << "vcpetrinet: annotation set not declared for " << annotation << endl;
            exit(1);
        }
        return it->second;
    }
    void annotatePNNode(PNNode *pnnode, PNAnnotation annotation)
    {
        getAnnotatedNodeset(annotation).insert(pnnode);
    }
#ifdef SIMU_MODE_STPN
private:
    DistFactory _df;
    PTerm *_defaultModelTerm;
    PDb _delaydb;
    map<int, IDistribution*> _distmap;
    void loadDelayModel()
    {
        string q_delaymodel = "delaymodel('" + _netname + "',X,Y).";
        string q_defaultmodel = "defaultmodel(X).";
        _delaydb.callStr( "delaymodel" , { q_delaymodel, q_defaultmodel } );
        auto dms = _delaydb.get( q_defaultmodel );
        if ( dms.size() != 1 )
        {
            cout << "Exactly 1 defaultmodel/1 must be specified" << endl;
            exit(1);
        }
        for( auto t : _delaydb.get( q_defaultmodel ) )
        {
            _defaultModelTerm = t->args()[0];
            break;
        }
        for( auto t : _delaydb.get( q_delaymodel ) )
        {
            auto tid = t->args()[0]->asInt();
            auto dist = t->args()[1];
            auto idist = _df.get(dist);
            _distmap.emplace(tid, idist);
        }
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

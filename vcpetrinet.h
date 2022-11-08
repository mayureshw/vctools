#ifndef _VCPETRINET_H
#define _VCPETRINET_H

#ifdef USECEP
#   include "cpp2xsb.h"
#   define PN_USE_EVENT_LISTENER
#endif

#include "petrinet.h"

// Purpose of this class is to build additional relations used for Prolog
// export in CEP. It makes no amends to the core PetriNet functionality.

using PetriNetVariant = MTPetriNet;
class VcPetriNet : public PetriNetVariant
{
using PetriNetVariant::PetriNetVariant;
public:
#ifdef USECEP
    Rel<unsigned,string,unsigned> vctid = {"vctid"};
#endif
};

#endif

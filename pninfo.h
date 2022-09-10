#ifndef _PNINFO_H
#define _PNINFO_H
#include "petrinet.h"

#ifdef PN_USE_EVENT_LISTENER
#include "cpp2xsb.h"
#endif

// Structure to accumulate information as various classes build the Petri net

class PNInfo
{
public:
    Elements pnes;
#ifdef PN_USE_EVENT_LISTENER
    Rel<unsigned,string,unsigned> vctid = {"vctid"};
#endif
};

#endif

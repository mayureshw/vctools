#ifndef _PNINFO_H
#define _PNINFO_H

#ifdef USECEP
#   include "cpp2xsb.h"
#   define PN_USE_EVENT_LISTENER
#endif

#include "petrinet.h"

// Structure to accumulate information as various classes build the Petri net

class PNInfo
{
public:
    Elements pnes;
#ifdef USECEP
    Rel<unsigned,string,unsigned> vctid = {"vctid"};
#endif
};

#endif

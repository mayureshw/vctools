#ifndef _PNINFO_H
#define _PNINFO_H
#include "petrinet.h"

#ifdef USECEP
#   include "cpp2xsb.h"
#endif

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

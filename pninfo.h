#ifndef _PNINFO_H
#define _PNINFO_H
#include "petrinet.h"
#include "cpp2xsb.h"

// Structure to accumulate information as various classes build the Petri net

class PNInfo
{
public:
    Elements pnes;
    Rel<unsigned,string,unsigned> vctid = {"vctid"};
};

#endif

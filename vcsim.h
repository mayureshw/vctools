#ifndef _VCSIM_H
#define _VCSIM_H

#include <set>
#include <map>
#include <string>
#include "datum.h"

class System;

static map<string,vector<DatumBase*>> emptymap;

// Main interface to vC simulator

// * * Please see `examples' directory for the usage of various parameters * *

void vcsim(
    // vC file name to simulate
    const string vcflnm,

    // The top module to be invoked for simulation
    const string invoke,

    // * * Following parameters are optional * *

    // Input parameter vector for the invoked module.
    const vector<DatumBase*>& inpv = {},

    // parameter `feeds' refers to the contents to be fed to the `system pipes'
    // from the test bench. A pair of pipe-name and contents to be fed to it
    // forms one 'feed'. A vector of such (possibly multiple) feeds contitutes
    // the `feeds' parameter
    const vector<pair<string,vector<DatumBase*>>>& feeds = {},

    // `collects' are just the opposite of `feeds' - meant to collect the
    // contents of the output system pipes populated. Since collecting data
    // from these pipes is a blocking operation, the number of reads needs to
    // be specified. A pair of pipename and the number of data elements to read
    // from it forms one `collect'. A vector of such (possibly multiple)
    // collects forms the `collects' parameter.
    const vector<pair<string,unsigned>>& collects = {},

    // A set of module names to be treated as daemon. These are started
    // automatically and are restarted in case they exit. They remain active
    // till end of simulation.
    const set<string>& daemons = {},

    // The results gathered as specified by the parameter `collects' are
    // returned as a map of pipename to a vector of gathered-contents from that
    // pipe is populated in the `collectopmap' parameter. The contents of these
    // pipes are logged. So if you just want to view them, no need to specify
    // this parameter.
    map<string,vector<DatumBase*>>& collectopmap = emptymap
    );

// Low level interface to vC simulator, see examples/vcsim04.cpp for usage of this
class VcsimIf
{
    System *_sys;
public:
    void stop();
    void invoke();
    void feedPipe(const string pipename, const vector<DatumBase*>& feedv);
    vector<DatumBase*> readPipe(const string pipename, unsigned cnt);
    vector<DatumBase*> oparamV(string modulename);
    VcsimIf(string vcflnm, const set<string>& daemons);
    ~VcsimIf();
};

#endif

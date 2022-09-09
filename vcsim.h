#ifndef _VCSIM_H
#define _VCSIM_H

#include <set>
#include <map>
using namespace std;

static map<string,vector<DatumBase*>> emptymap;

void vcsim(const string vcflnm, const string invoke, const vector<DatumBase*>& inpv,
    const vector<pair<string,vector<DatumBase*>>>& feeds = {},
    const vector<pair<string,unsigned>>& collects = {},
    const set<string>& daemons = {},
    map<string,vector<DatumBase*>>& collectopmap = emptymap
    );

#endif

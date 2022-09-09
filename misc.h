#ifndef _UTILS_H
#define _UTILS_H

#include <set>
#include <map>
#include <functional>
#include <vector>
#include <cassert>
using namespace std;
using namespace std::placeholders;
// Usage: if there is a single constructor for the value, easier to pass it at
// construction time and then use []. If the construction method varies, do not
// pass the constructor at construction time and do not use []. Pass it to get.

template <typename KT, typename VT> class internmap
{
    function<VT(KT)> _constructor;
    map<KT,VT> _map;
public:
    internmap(function<VT(KT)> constructor = NULL) : _constructor(constructor) {}
    VT operator [] (KT k)
    {
        assert(_constructor);
        return get(k,_constructor);
    }
    VT get(KT k, function<VT(KT)> constructor)
    {
        auto it = _map.find(k);
        if (it != _map.end() ) return it->second;
        VT newval = constructor(k);
        _map[k] = newval;
        return newval;
    }
    bool found(KT k) { return _map.find(k) != _map.end(); }
    map<KT,VT>& getmap() { return _map; }
};


template <typename T> class Uid
{
    internmap<T,unsigned> _nodeids;
    map<unsigned,T> _idnodes;
    unsigned newid(T key)
    {
        unsigned mapsz = _nodeids.getmap().size();
        _idnodes.emplace(mapsz, key);
        return mapsz;
    }
public:
    unsigned size() { return _idnodes.size(); }
    unsigned getid(T t) { return _nodeids[t]; }
    T getnode(unsigned id) { return _idnodes[id]; }
    Uid() : _nodeids(bind(&Uid::newid,this,_1)) {}
};
#endif

#ifndef _GRAPH_H
#define _GRAPH_H

#include <map>
#include <list>
#include <iostream>
#include "matrix.h"
#include "misc.h"

using namespace std;

template <typename T> class Graph
{
    SqMatrix<bool> _adjm;
    Uid<T>& _nodeids;
protected:
    unsigned _nodecnt;
    void sizechk(Graph<T>& othergraph)
    {
        if ( _nodecnt != othergraph._nodecnt )
        {
            cout << "Graph operation invoked on different sized graph " << _nodecnt << " " << othergraph._nodecnt;
            exit(1);
        }
    }
public:
    typedef list<pair<T,vector<T>>> t_adjlist;
    void dump()
    {
        for(unsigned i=0; i<_nodecnt; i++)
        {
            T n = _nodeids.getnode(i);
            cout << n << " <- ";
            for(unsigned j=0; j<_nodecnt; j++)
            {
                if(_adjm[i][j])
                {
                    T nbrnode = _nodeids.getnode(j);
                    cout << nbrnode << ",";
                }
            }
            cout << endl;
        }
    }
    bool exists_ij(unsigned i, unsigned j) { return _adjm[i][j]; }
    bool exists(T n1, T n2)
    {
        auto n1id = _nodeids.getid(n1);
        auto n2id = _nodeids.getid(n2);
        return exists_ij(n1id, n2id);
    }
    void addAdjFrom(unsigned thisi, Graph<T>& othergraph, unsigned otheri)
    {
        sizechk(othergraph);
        for(int j=0; j<_nodecnt; j++)
        // tiny saving of write operations by `if'
        if ( not _adjm[thisi][j] and othergraph._adjm[otheri][j] )
            _adjm[thisi][j] = true;
    }
    void operator - (Graph<T>& othergraph)
    {
        sizechk(othergraph);
        for(unsigned i=0; i<_nodecnt; i++)
        for(unsigned j=0; j<_nodecnt; j++)
        if ( _adjm[i][j] and othergraph._adjm[i][j] )
            _adjm[i][j] = false;
    }
    void closure()
    {
        for(unsigned k=0; k<_nodecnt; k++)
        for(unsigned i=0; i<_nodecnt; i++)
        for(unsigned j=0; j<_nodecnt; j++)
        // tiny saving of write operations by `if'
        if ( not _adjm[i][j] ) _adjm[i][j] = _adjm[i][k] and _adjm[k][j];
    }
    void getAdjList(t_adjlist& adjlist)
    {
        for(unsigned i=0; i<_nodecnt; i++)
        {
            vector<T> adjnodes;
            for(unsigned j=0; j<_nodecnt; j++)
                if ( _adjm[i][j] )
                {
                    T jthnode = _nodeids.getnode(j);
                    adjnodes.push_back(jthnode);
                }
            T ithnode = _nodeids.getnode(i);
            adjlist.push_back({ithnode,adjnodes});
        }
    }
    unsigned edgeCnt()
    {
        unsigned cnt = 0;
        for(unsigned i=0; i<_nodecnt; i++)
        for(unsigned j=0; j<_nodecnt; j++)
        if ( _adjm[i][j] ) cnt++;
        return cnt;
    }
    unsigned nodeCnt() { return _nodecnt; }
    Graph(t_adjlist& adjlist, Uid<T>& nodeids) :
        _nodecnt(adjlist.size()), _adjm(adjlist.size(),false), _nodeids(nodeids)
    {
        for(auto adjp:adjlist)
        {
            auto i = _nodeids.getid(adjp.first);
            for(auto nbr:adjp.second)
            {
                auto j = _nodeids.getid(nbr);
                if ( j >= _nodecnt or i >= _nodecnt )
                {
                    cout << "nodeid exceeds node count " << i << "," << j << " " << _nodecnt << endl;
                    exit(1);
                }
                _adjm[i][j] = true;
            }
        }
    }
};

#endif

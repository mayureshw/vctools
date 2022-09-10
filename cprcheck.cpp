using namespace std;

#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <functional>
#include <string>
#include "graph.h"
#include "vcLexer.hpp"
#include "vcParser.hpp"

class ModuleDeps
{
    typedef Graph<vcCPElement*> t_cpegraph;
    typedef set<vcCPElement*> t_cpeset;
    typedef t_cpegraph::t_adjlist t_adjlist;
    vcModule *_vcm;
    t_cpegraph *_pre_nonmarked_deps;
    t_cpegraph *_pre_nonmarked_deps_t;
    t_cpegraph *_pre_marked_deps;
    t_cpegraph *_pre_marked_deps_t;
    t_cpegraph *_post_nonmarked_deps;
    t_cpegraph *_post_marked_deps;
    Uid<vcCPElement*> _nodeids;
    string depstr(vcCPElement *n, vcCPElement *d)
    {
        return n->Get_Hierarchical_Id() + " <- " + d->Get_Hierarchical_Id();
    }
    // TODO: Not sure of the authentic way to build universal set, this is a bit adhoc
    void buildUnivSet(vcCPBlock *blk, t_cpeset& univset)
    {
        univset.insert(blk->Get_Entry_Element());
        univset.insert(blk->Get_Exit_Element());
        for(auto cpe:blk->Get_Elements())
        {
            univset.insert(cpe);
            if ( cpe->Is_Block() ) buildUnivSet( (vcCPBlock*) cpe, univset);
        }
    }
    void printAdjList(t_adjlist& al)
    {
        for(auto adjp:al) for(auto nbr:adjp.second)
            cout << "\t" << depstr(adjp.first, nbr) << endl;
    }
public:
    string modulename() { return _vcm->Get_Id(); }
    void check()
    {
        t_cpegraph post_nm_extra = *_post_nonmarked_deps;
        t_cpegraph post_m_extra = *_post_marked_deps;
        post_nm_extra - *_pre_nonmarked_deps_t;
        post_m_extra - *_pre_marked_deps_t;
        t_adjlist post_nmextra_adjlist, post_mextra_adjlist;
        post_nm_extra.getAdjList(post_nmextra_adjlist);
        post_m_extra.getAdjList(post_mextra_adjlist);
        cout << modulename() << " _post_nonmarked_deps - _pre_nonmarked_deps_t:" << post_nm_extra.edgeCnt() << "/" << _post_nonmarked_deps->edgeCnt() << endl;
        printAdjList( post_nmextra_adjlist );
        cout << modulename() << " _post_marked_deps - _pre_marked_deps:" << post_m_extra.edgeCnt() << "/" << _post_marked_deps->edgeCnt() << endl;
        printAdjList( post_mextra_adjlist );
    }
    void checkUnivset(t_cpeset& univset)
    {
        for(auto cpe:univset)
        {
            vector<vcCPElement*> flattendpreds;
            getFlattendPreds(cpe, flattendpreds);
            for(auto pred:flattendpreds)
            if(univset.find(pred)==univset.end())
                cout << "Not found in univ set " << pred->Kind() << ":" << pred->Get_Hierarchical_Id() << endl;
        }

        auto cp = _vcm->Get_Control_Path();
        auto egmap = cp->Get_CPElement_To_Group_Map();
        for(auto egpair:egmap)
            if ( egpair.second )
                for(auto e:egpair.second->Get_Elements())
        if(univset.find(e)==univset.end())
            cout << "Not found in univ set " << e->Get_Hierarchical_Id() << endl;
    }
    void getFlattendPreds(vcCPElement* cpe, vector<vcCPElement*>& flattendpreds)
    {
        if ( cpe->Is_Transition() )
            if ( ((vcTransition*) cpe)->Get_Is_Entry_Transition() )
            {
                auto parent = cpe->Get_Parent();
                assert(parent);
                if ( parent != _vcm->Get_Control_Path() )
                    flattendpreds.push_back( parent );
            }
        for(auto pred:cpe->Get_Predecessors())
        {
            auto flatpred = pred->Is_Block() ? pred->Get_Exit_Element() : pred;
            flattendpreds.push_back( flatpred );
        }
    }
    ModuleDeps(vcModule* vcm) : _vcm(vcm)
    {
        t_cpeset univset;
        auto cp = _vcm->Get_Control_Path();
        auto egmap = cp->Get_CPElement_To_Group_Map();
        buildUnivSet(cp, univset);
        checkUnivset(univset); // Only for development purposes
        t_adjlist pre_nm_adjlist, pre_m_adjlist, post_nm_adjlist, post_m_adjlist;
        for(auto cpe:univset)
        {
            // pre graphs
            vector<vcCPElement*> flattendpreds;
            getFlattendPreds(cpe, flattendpreds);
            pre_nm_adjlist.push_back({cpe,flattendpreds});
            pre_m_adjlist.push_back({cpe,cpe->Get_Marked_Predecessors()});

            // post graphs
            auto it = egmap.find(cpe);
            // TODO group found to be NULL sometimes, report
            if ( it == egmap.end() or it->second == NULL )
            {
                post_nm_adjlist.push_back({cpe,{}});
                post_m_adjlist.push_back({cpe,{}});
            }
            else
            {
                auto grp = it->second;
                vector<vcCPElement*> nm_preds_via_g;
                vector<vcCPElement*> m_preds_via_g;
                for(auto predg: grp->Get_Predecessors())
                    for(auto eviag: predg->Get_Elements())
                        nm_preds_via_g.push_back(eviag);
                for(auto predg: grp->Get_Marked_Predecessors())
                    for(auto eviag: predg->Get_Elements())
                        m_preds_via_g.push_back(eviag);
                post_nm_adjlist.push_back({cpe, nm_preds_via_g});
                post_m_adjlist.push_back({cpe, m_preds_via_g});
            }
        }

        _pre_nonmarked_deps = new t_cpegraph(pre_nm_adjlist, _nodeids);
        _pre_marked_deps = new t_cpegraph(pre_m_adjlist, _nodeids);
        _post_nonmarked_deps = new t_cpegraph(post_nm_adjlist, _nodeids);
        _post_marked_deps = new t_cpegraph(post_m_adjlist, _nodeids);

        _pre_nonmarked_deps_t = new t_cpegraph(*_pre_nonmarked_deps);
        _pre_nonmarked_deps_t->closure();

        _pre_marked_deps_t = new t_cpegraph(*_pre_marked_deps);
        for( unsigned i=0; i<_pre_marked_deps->nodeCnt(); i++ )
        for( unsigned j=0; j<_pre_marked_deps->nodeCnt(); j++)
        if( _pre_marked_deps->exists_ij(i,j) )
            _pre_marked_deps_t->addAdjFrom(i, *_pre_nonmarked_deps_t, j);

    }
    ~ModuleDeps()
    {
        delete _pre_nonmarked_deps;
        delete _pre_marked_deps;
        delete _post_nonmarked_deps;
        delete _post_marked_deps;
        delete _pre_nonmarked_deps_t;
        delete _pre_marked_deps_t;
    }
};

class SysDeps
{
    list<ModuleDeps*> _mdeps;
public:
    void check() { for(auto mdep:_mdeps) mdep->check(); }
    SysDeps(vcSystem& vcs)
    {
        for(auto m:vcs.Get_Modules()) _mdeps.push_back(new ModuleDeps(m.second));
    }
    ~SysDeps() { for(auto md:_mdeps) delete md; }
};

int main(int argc, char *argv[])
{
    ifstream stream;
    if ( argc != 2 )
    {
        cout << "Usage: " << argv[0] << " <vcfile> " << endl;
        exit(1);
    }
    char* vcflnm = argv[1];
    stream.open(vcflnm);
    if ( not stream.is_open() )
    {
        cout << "Could not open file: " << vcflnm << endl;
        exit(1);
    }
    vcSystem::_opt_flag = true;

    vcSystem vcs("sys");
    vcs.Parse(vcflnm);
    for(auto m:vcs.Get_Modules()) vcs.Set_As_Top_Module(m.second);
    vcs.Elaborate();

    SysDeps sysdeps(vcs);
    sysdeps.check();
}

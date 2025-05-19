#ifndef _VC2PN_H
#define _VC2PN_H

#include <map>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "vcLexer.hpp"
#include "vcParser.hpp"
#include "misc.h"
#include "dot.h"
#include "vcpetrinet.h"
#include "operators.h"
#include "pipes.h"
#include "opf.h"
#include "vc2pnbase.h"
#include "opfactory.h"

#ifdef USECEP
#   include "intervals.h"
#endif

using namespace std::placeholders;

// Bridge classes between vc IR and Petri net simulator
// Can use inheritance as long as we don't need extra attribs, else use
// containment
// TODO: Possibly more room to abstract to Element. Dealing with
// statics by use of template leads to some baffling issues such as template<>
// syntax and linking errors.
class Element
{
protected:
    string _label;
    ModuleBase* _module;
    vcRoot* _elem;
    virtual vcRoot* elem() = 0;
public:
    string label() { return _label; }
    SystemBase* sys() { return _module->sys(); }
    VcPetriNet* pn() { return _module->pn(); }
    bool inVolatileModule() { return _module->isVolatile(); }
    Element(vcRoot* elem, ModuleBase* module) : _elem(elem), _module(module) {}
    virtual void buildPN()=0;
};

class DPElement : public Element
{
    const bool _isGuarded;
    vector<PNTransition*> _reqs, _acks;
    vector<PNTransition*> _greqs, _gacks;
    Operator *_op;
    BRANCH *_guardBranchOp;
    PNPlace *_branchPlace = NULL; // associated with guarded nodes and branch operators
    void indexcheck(int indx)
    {
        if ( (indx!=0) && (indx!=1) )
        {
            cout << "vc2pn: indx other than 0, 1 not handled (todo)" << endl;
            exit(1);
        }
    }
public:
    Operator *getOp() { return _op; }
    Operator *getDriverOp( vcWire *w )
    {
        auto driver = w->Get_Driver();
        assert(driver);
        return _module->getDPE(driver)->getOp();
    }
    const vector<PNTransition*>& getReqs() { return _reqs; }
    const vector<PNTransition*>& getAcks() { return _acks; }
    const vector<PNTransition*>& getGReqs() { return _greqs; }
    const vector<PNTransition*>& getGAcks() { return _gacks; }
    vcDatapathElement* elem() { return (vcDatapathElement*) _elem; }
    VCtyp _vctyp;
    PNTransition* getReqTransition(int indx)
    {
        vector<PNTransition*>& reqs = isDeemedGuarded() ? _greqs : _reqs;
        if ( indx >= reqs.size() )
        {
            cout << "getReqTransition indx=" << indx << " >= reqs.size=" << reqs.size() << " " << label() << endl;
            exit(1);
        }
        return reqs[indx];
    }
    PNTransition* getAckTransition(int indx)
    {
        vector<PNTransition*>& acks = isDeemedGuarded() ? _gacks : _acks;
        if ( indx >= acks.size() )
        {
            cout << "getAckTransition indx=" << indx << " >= acks.size=" << acks.size() << " " << label() << endl;
            exit(1);
        }
        return acks[indx];
    }

    // General guideline for buildPN* functions: Retain the CP interface defined by _reqs and _acks as-is
    // Can improvise a network (e.g. additional places/transitions) within those boundaries
    void buildPNPhi()
    {
        auto phplace = pn()->createPlace("DPE:" + _label + "_phplace");
        auto rootindex = elem()->Get_Root_Index();
        for(int i=0; i<_reqs.size(); i++)
        {
            auto req = _reqs[i];
            pn()->createArc(req, phplace);
#           ifdef USECEP
            pn()->vctid.add({ rootindex, "req" + to_string(i), req->_nodeid });
#           endif
            req->setEnabledActions(bind(&Operator::select,_op,i,_1));
        }
        pn()->createArc(phplace, _acks[0]);
#       ifdef USECEP
        pn()->vctid.add({ rootindex, "ack0", _acks[0]->_nodeid });
#       endif
        _acks[0]->setEnabledActions(bind(&Operator::uack,_op,_1));

    }
    void createBrPlaceAcks()
    {
        _branchPlace = pn()->createPlace("DPE:" + _label + "_brplace");
        pn()->annotatePNNode(_branchPlace, Branch_);
        _branchPlace->setArcChooser(bind(&BRANCH::arcChooser,(BRANCH*)_op));
        pn()->createArc(_branchPlace, _acks[0]);
        pn()->createArc(_branchPlace, _acks[1]);
#       ifdef USECEP
        auto rootindex = elem()->Get_Root_Index();
        pn()->vctid.add({ rootindex, "ack0", _acks[0]->_nodeid });
        pn()->vctid.add({ rootindex, "ack1", _acks[1]->_nodeid });
#       endif
    }
    void buildPNBranch()
    {
        auto inpwires = elem()->Get_Input_Wires();
        if ( inpwires.size() != 1 )
        {
            cout << "vc2pn: branch has input driver count != 1: " << elem()->Get_Id() << endl;
            exit(1);
        }
        createBrPlaceAcks();
        pn()->createArc(_reqs[0], _branchPlace);
#       ifdef USECEP
        auto rootindex = elem()->Get_Root_Index();
        pn()->vctid.add({ rootindex, "req0", _reqs[0]->_nodeid });
#       endif
    }
    int uackIndx()
    {
        return _vctyp == vcPhiPipelined_ ? 0 : 1;
    }
    PNPlace* branchPlace()
    {
        if ( _branchPlace == NULL )
        {
            cout << "vc2pn: invalid branchPlace access" << endl;
            exit(1);
        }
        return _branchPlace;
    }
    vcWire* branchInpWire()
    {
        if ( isDeemedGuarded() ) return elem()->Get_Guard_Wire();
        // buildPNBranch already validates the vector size of input wires to be 1
        else if ( isBranch() ) return elem()->Get_Input_Wires()[0];
        else
        {
            cout << "vc2pn: invalid branchInpWire access" << endl;
            exit(1);
        }
    }
    bool isIport() { return _vctyp == vcInport_; }
    bool isOport() { return _vctyp == vcOutport_; }
    bool isCall() { return _vctyp == vcCall_; }
    bool isLoad() { return _vctyp == vcLoad_; }
    bool isStore() { return _vctyp == vcStore_; }
    bool isBranch() { return _vctyp == vcBranch_; }
    bool isSlice() { return _vctyp == vcSlice_; }
    bool isDeemedPhi()
    {
        return _vctyp == vcPhi_ or _vctyp == vcPhiPipelined_;
    }
    bool isDeemedFlowThrough()
    {
        return elem()->Get_Flow_Through() or isSignalInport() or inVolatileModule();
    }
    bool isDeemedGuarded()
    {
        return _isGuarded and not isDeemedFlowThrough();
    }
    bool isSignalInport()
    {
        return _vctyp == vcInport_ and (((vcIOport*)elem())->Get_Pipe())->Get_Signal();
    }
    void wrapPNWithGuard()
    {
        auto dpelabel = "DPE:" + _label + "_";
        PNTransition *go = pn()->createTransition(dpelabel + "go");
        PNTransition *nogo = pn()->createTransition(dpelabel + "nogo");
        PNTransition *nogo_ureq = pn()->createTransition(dpelabel + "nogo_ureq");
        _branchPlace = pn()->createPlace(dpelabel + "brplace");
        pn()->annotatePNNode(_branchPlace, Branch_);
        _branchPlace->setArcChooser(bind(&BRANCH::arcChooser, _guardBranchOp));
        PNPlace *gsackplace = pn()->createPlace(dpelabel + "gsackplace");
        PNPlace *guackplace = pn()->createPlace(dpelabel + "guackplace");
        PNPlace *gureqplace = pn()->createPlace(dpelabel + "gureqplace");
        pn()->annotatePNNode(gureqplace, PassiveBranch_);

        auto gsreq = _greqs[0], gureq = _greqs[1], gsack = _gacks[0], guack = _gacks[1];
        auto sreq = _reqs[0], ureq = _reqs[1], sack = _acks[0], uack = _acks[1];

        pn()->createArc(gsreq, _branchPlace);

        if ( elem()->Get_Guard_Complement() )
        {
            pn()->createArc(_branchPlace, go);
            pn()->createArc(_branchPlace, nogo);
        }
        else
        {
            pn()->createArc(_branchPlace, nogo);
            pn()->createArc(_branchPlace, go);
        }
        pn()->createArc(nogo, gsackplace);
        pn()->createArc(nogo, nogo_ureq);
        pn()->createArc(nogo_ureq, guackplace);
        pn()->createArc(go, sreq);
        pn()->createArc(go, ureq);
        pn()->createArc(sack, gsackplace);
        pn()->createArc(uack, guackplace);
        pn()->createArc(gsackplace, gsack);
        pn()->createArc(guackplace, guack);
        pn()->createArc(gureq, gureqplace);
        pn()->createArc(gureqplace, ureq);
        pn()->createArc(gureqplace, nogo_ureq);
    }
    void trimIportSackUreqPath()
    {
        // This is kept for experimental purposes only. Inport sreq-sack-ureq path is made so as to control pipe read request
        // from the incoming flow, rather than leaving it entirely to reverse request from the consumer
        auto n = (PNNode*) _acks[0];
        const int sack2ureqDist = 3;
        for( int i=0; i<sack2ureqDist; i++ )
        {
            auto oarcs = n->_oarcs;
            if ( oarcs.size() != 1 )
            {
                cout << "trimIportSackUreqPath expects a straight line path. Got oarcs.size=" << oarcs.size() << " in " << n->idlabel() << endl;
                exit(1);
            }
            n = oarcs[0]->target();
            pn()->annotatePNNode(n,SimuOnly_);
        }
    }
    // buildPN that suits most DPEs
    void buildPNDefault()
    {
        if ( isIport() ) // In AHIR Inport reads on ureq, not on sreq
        {
            _reqs[1]->setEnabledActions(bind(&Operator::ureq,_op,_1));
            // trimIportSackUreqPath(); // Experimental only
        }
        else
            _acks[0]->setEnabledActions(bind(&Operator::sack,_op,_1));
        _acks[1]->setEnabledActions(bind(&Operator::uack,_op,_1));
        auto sreq_sack_place = pn()->createArc(_reqs[0], _acks[0]);
        pn()->annotatePNNode(sreq_sack_place, SimuOnly_);
        auto ureq_uack_place = pn()->createArc(_reqs[1], _acks[1]);
        pn()->annotatePNNode(ureq_uack_place, SimuOnly_);

        auto sack2ureq = pn()->createArc(_acks[0], _reqs[1]); // Since sreq/ureq can be ||, need this sync
        pn()->annotatePNNode( sack2ureq, SimuOnly_ );
        auto uack2sreq = (PNPlace*) pn()->createArc(
            _acks[1], _reqs[0],
            "MARKP:" + _reqs[0]->_name
            );
        // Choice of making uack2sreq SimuOnly_ has varied. As of last revision the idea is to localize this choice in RTL
        pn()->annotatePNNode(uack2sreq, SimuOnly_);
        uack2sreq->setMarking(1);
        auto rootindex = elem()->Get_Root_Index();
#       ifdef USECEP
        pn()->vctid.add({ rootindex, "req0", _reqs[0]->_nodeid });
        pn()->vctid.add({ rootindex, "req1", _reqs[1]->_nodeid });
        pn()->vctid.add({ rootindex, "ack0", _acks[0]->_nodeid });
        pn()->vctid.add({ rootindex, "ack1", _acks[1]->_nodeid });
#       endif
        if ( isIport() or isOport() )
        {
            auto sreq = _reqs[0];
            auto sack = _acks[0];
            auto ureq = _reqs[1];
            auto uack = _acks[1];
            auto pipe = ((IOPort*)_op)->_pipe;
            if ( isIport() ) pipe->buildPNIport(ureq, uack); // See comment "In AHIR..." above
            else pipe->buildPNOport(sreq, sack);
        }
        else if ( isCall() )
        {
            auto sreq = _reqs[0];
            auto sack = _acks[0];
            auto uack = _acks[1];

            auto calledVcModule = ((vcCall*)_elem)->Get_Called_Module();
            auto calledModule = sys()->getModule( calledVcModule );
            auto calledExitPlace = calledModule->exitPlace();
            auto calledEntryPlace = calledModule->entryPlace();
            auto calledMutexPlace = calledModule->mutexPlace();

            auto inProgressPlace = pn()->createPlace(_label+".CallInProgress");
            // sreq requires called module's mutex token
            pn()->createArc(calledMutexPlace, sreq);
            // and passes token to inProgressPlace and called module's entry
            pn()->createArc(sreq, inProgressPlace);
            pn()->createArc(sreq, calledEntryPlace);
            // uack requires inProgressPlace and called module's exit place token
            pn()->createArc(inProgressPlace, uack);
            pn()->createArc(calledExitPlace, uack);
            // and releases a token to called module's mutex
            pn()->createArc(uack, calledMutexPlace);
        }
        else if ( isLoad() or isStore() )
        {
            auto sreq = _reqs[0];
            auto uack = _acks[1];
            auto storageMutexPlace = sys()->getStorageMutexPlace( elem() );
            pn()->createArc(storageMutexPlace,sreq);
            pn()->createArc(uack,storageMutexPlace);
        }
    }
    void createGuardReqs()
    {
        for(int i=0; i<elem()->Get_Number_Of_Reqs(); i++)
            _greqs.push_back(pn()->createTransition("DPE:" + _label + "_greq_" + to_string(i)));
    }
    void createGuardAcks()
    {
        for(int i=0; i<elem()->Get_Number_Of_Reqs(); i++)
            _gacks.push_back(pn()->createTransition("DPE:" + _label + "_gack_" + to_string(i)));
    }
    void createReqs()
    {
        for(int i=0; i<elem()->Get_Number_Of_Reqs(); i++)
            _reqs.push_back(pn()->createTransition("DPE:" + _label + "_req_" + to_string(i)));
    }
    void createAcks()
    {
        for(int i=0; i<elem()->Get_Number_Of_Acks(); i++)
            _acks.push_back(pn()->createTransition("DPE:" + _label + "_ack_" + to_string(i)));
    }
    void buildPN()
    {
        if ( isDeemedFlowThrough() );
        else if ( isBranch() )
            buildPNBranch();
        else if ( isDeemedPhi() )
            buildPNPhi();
        else if ( ( _reqs.size() == 2 ) && ( _acks.size() == 2 ) ) // Default handling
            buildPNDefault();
        else
        {
            cout << "vc2pn: unhandled req-ack pattern reqs=" << _reqs.size() << " acks=" << _acks.size() << " " << _elem->Kind() << " " << _elem->Get_Id()  << endl;
            exit(1);
        }
        if ( isDeemedGuarded() ) wrapPNWithGuard();
    }

    void setGuardBranchInpv()
    {
        vector<DatumBase*> inpv;
        auto w = elem()->Get_Guard_Wire();
        DatumBase* inpdatum;
        if ( w->Kind() == "vcInputWire" )
            inpdatum = _module->inparamDatum( w->Get_Id() );
        else if ( w->Is_Constant() )
            inpdatum = sys()->valueDatum( ((vcConstantWire*) w)->Get_Value() );
        else
            inpdatum = _module->opregForWire(w);
        inpv.push_back(inpdatum);
        _guardBranchOp->setinpv(inpv);
    }
    void handleSignalInportFT()
    {
        auto owires = elem()->Get_Output_Wires();
        if ( owires.size() != 1 )
        {
            cout << "Signal with output wires count != 1 not expected" << endl;
            exit(1);
        }
        auto receivers = owires[0]->Get_Receivers();
        if ( receivers.size() != 1 )
        {
            cout << "Signal output wire with receivers count != 1 not expected" << endl;
            exit(1);
        }
        auto succelem = *receivers.begin();
        auto succdpe = _module->getDPE(succelem);
        auto succ_sreq = succdpe->getReqTransition(0);
        succ_sreq->setEnabledActions(bind(&Operator::flowthrough,_op,_1));
    }
    void setinpv()
    {
        vector<DatumBase*> inpv;
        set<Operator*> listenToOps;
        for( auto w : elem()->Get_Input_Wires() )
        {
            DatumBase* inpdatum;
            if ( w->Kind() == "vcInputWire" )
            {
                inpdatum = _module->inparamDatum( w->Get_Id() );
                if ( isDeemedFlowThrough() )
                    _module->registerFTListener( _op );
            }
            else if ( w->Is_Constant() )
                inpdatum = sys()->valueDatum( ((vcConstantWire*) w)->Get_Value() );
            else
            {
                inpdatum = _module->opregForWire(w);
                if ( isDeemedFlowThrough() )
                    listenToOps.insert( getDriverOp(w) );
            }
            inpv.push_back(inpdatum);
        }
        _op->setinpv(inpv);

        if ( isDeemedFlowThrough() and listenToOps.size() > 0 )
            _op->setFlowthroughInps( listenToOps );

        if ( isSignalInport() ) handleSignalInportFT();

        if ( isDeemedGuarded() ) setGuardBranchInpv();
    }
    void setGuardBranchOp()
    {
        _guardBranchOp = new BRANCH(0, label()+"_guardc");
    }
    vector<DatumBase*>& opv() { return _op->opv; }

    DPElement(vcDatapathElement* elem, ModuleBase* module) : Element(elem, module), _isGuarded(elem->Get_Guard_Wire()!=NULL)
    {
        _label = _module->name() + "/" + _elem->Get_Id();
        _vctyp = sys()->vctyp( _elem->Kind() );

        // Some createReqs need _op initialized, so this is before createReqs
        _op = sys()->createOperator(elem);
        if ( not isDeemedFlowThrough() )
        {
            createReqs();
            createAcks();
        }
        if ( isDeemedGuarded() )
        {
            if ( elem->Get_Number_Of_Reqs() != 2 or elem->Get_Number_Of_Acks() != 2 )
            {
                cout << "Guarded DPE with req/ack count !=2 not handled " << _label << " Reqs=" << elem->Get_Number_Of_Reqs() << " Acks=" << elem->Get_Number_Of_Acks() << endl;
                exit(1);
            }
            createGuardReqs();
            createGuardAcks();
            setGuardBranchOp();
        }
    }
    ~DPElement()
    {
        delete _op;
        if ( isDeemedGuarded() ) delete _guardBranchOp;
    }
};

class CPElement : public Element
{
protected:
    PNNode* _pnnode;
public:
    CPElement(vcRoot* elem, ModuleBase* module) : Element(elem, module) {}

    // out and in is with respect to this node. This node's successors should
    // call outPNNode and predecessors should call inPNNode In most cases they
    // are the same. But in some cases where they are different the respective
    // CPElement subclass will override these methods.
    virtual PNNode* outPNNode() { return _pnnode; }
    virtual PNNode* inPNNode() { return _pnnode; }
    // In cases where in/out doesn't matter (indicated by flag _hasSinglePNNode)
    // one can access pnNode. If the flag is false, it's a run-time error.
    PNNode* pnNode()
    {
        if ( _pnnode != NULL ) return _pnnode;
        else
        {
            cout << "pnNode sought when NULL. Possibly you need in/out variant of the api" << endl;
            exit(1);
        }
    }
};

class GroupCPElement : public CPElement
{
    virtual string shape()=0;
    virtual string color()=0;
    void dplinksizecheck(int vectsz)
    {
        if ( vectsz != 1 )
        {
            cout << "vc2pn: dplink sizes other than 1 are not handled (todo)" << endl;
            exit(1);
        }
    }
    vcDatapathElement* vct2vcdpe(vcTransition* vct)
    {
        vector<pair<vcDatapathElement*,vcTransitionType> >& vct_dpl =
            vct->Get_DP_Link();
        dplinksizecheck(vct_dpl.size());
        vcDatapathElement* vcdpe = vct_dpl[0].first;
        return vcdpe;
    }
protected:
    vcCPElementGroup* elem() { return (vcCPElementGroup*) _elem; }
    // sub-classes that have some internal arcs to build should override this
    virtual void buildPNlocal()
    {
    }
public:
    void buildPN()
    {
        buildPNlocal();
        // TODO: Take input and output transition first, then other things
        // from transition take its DPE and index, from that DPE and index take corresp transition from DPE
        if ( elem()->Get_Has_Input_Transition() )
        {
            vcTransition *inpt = elem()->Get_Input_Transition();
            vcDatapathElement* vcdpe = vct2vcdpe(inpt);
            DPElement *dpe = _module->getDPE(vcdpe);
            int ackindx = vcdpe->Get_Ack_Index(inpt);
            PNTransition *inppnt = dpe->getAckTransition(ackindx);
            pn()->createArc(inppnt, inPNNode());
        }
        else
        {
            for(auto predeg: elem()->Get_Predecessors())
            {
                auto *predcpe = (GroupCPElement*) _module->getCPE(predeg);
                pn()->createArc(predcpe->outPNNode(), inPNNode());
            }
            // TODO: Temporarily replicating _predecessor logic for _marked_predecessors
            for(auto predeg: elem()->Get_Marked_Predecessors())
            {
                auto *predcpe = (GroupCPElement*) _module->getCPE(predeg);
                auto markedplace = (PNPlace*) pn()->createArc(
                    predcpe->outPNNode(), inPNNode(),
                    "MARKP:" + inPNNode()->_name
                    );
                markedplace->setMarking(1);
            }
        }
        if ( elem()->Get_Has_Output_Transition() )
        {
            vector<vcTransition*> optransitions = elem()->Get_Output_Transitions();
            for(auto optran: optransitions)
            {
                vcDatapathElement* vcdpe = vct2vcdpe(optran);
                DPElement *dpe = _module->getDPE(vcdpe);
                int reqindx = vcdpe->Get_Req_Index(optran);
                PNTransition *optrant = dpe->getReqTransition(reqindx);
                pn()->createArc(outPNNode(), optrant);
            }
        }
        // NOTE: _pnnode serves as output end, hence below code won't work for
        // some nodes (e.g. those with fork and merge combined)
        //
        //else
        //    for(auto succeg: _elem->_successors)
        //    {
        //        CPElement *succcpe = _module->getCPE(succeg);
        //        pn()->createArc(_pnnode, succcpe);
        //    }
        //if ( _elem->_is_join or _elem->Get_Is_Merge() )
        //{
        //    for(auto predeg: _elem->Get_Predecessors())
        //    {
        //        CPElement *predcpe = _module->getCPE(predeg);
        //        pn()->createArc(predcpe->_pnnode);
        //    }
        //}
    }
    GroupCPElement(vcCPElementGroup* elemg, ModuleBase* module) : CPElement(elemg, module)
    {
        _label = module->name()+"/";
        //for(auto e:elemg->Get_Elements()) _label += e->Get_Hierarchical_Id() + ",";
        for(auto e:elemg->Get_Elements()) _label += e->Get_Id() + ",";
    }
};

class PlaceElement : public GroupCPElement
{
public:
    string shape() { return "ellipse"; }
    string color() { return "green"; }
    PlaceElement(vcCPElementGroup* elemg, ModuleBase* module) : GroupCPElement(elemg, module)
    {
        _pnnode = pn()->createPlace("CPE:"+to_string(elem()->Get_Group_Index())+":"+label());
    }
};

class TransElement : public GroupCPElement
{
public:
    string shape() { return "rectangle"; }
    string color() { return "blue"; }
    TransElement(vcCPElementGroup* elemg, ModuleBase* module) : GroupCPElement(elemg, module)
    {
        _pnnode = pn()->createTransition("CPE:"+to_string(elem()->Get_Group_Index())+":"+label());
    }
};

// Overloaded GroupCPElement where Place is input and transition is output
class PTElement : public GroupCPElement
{
    PNNode *_inPNNode;
    PNNode *_outPNNode;
public:
    // TODO: shape / color not in use and how to use for this kind of classes anyway?
    string shape() { return "rectangle"; }
    string color() { return "blue"; }
    PNNode* inPNNode() { return _inPNNode; }
    PNNode* outPNNode() { return _outPNNode; }
    void buildPNlocal()
    {
        pn()->createArc(_inPNNode, _outPNNode);
    }
    PTElement(vcCPElementGroup* elemg, ModuleBase* module) : GroupCPElement(elemg, module)
    {
        _pnnode = NULL;
        _inPNNode = pn()->createPlace("CPE:P:"+to_string(elem()->Get_Group_Index())+":"+label());
        _outPNNode = pn()->createTransition("CPE:T:"+to_string(elem()->Get_Group_Index())+":"+label());
    }
};

class SoloCPElement : public CPElement
{
protected:
    PNNode* vce2pnnode(vcCPElement* vce) { return _module->getCPE(vce)->pnNode(); }
public:
    PNNode* _pnnode;
    string shape() { return "rectangle"; }
    string color() { return "blue"; }
    SoloCPElement(vcCPElement *elem, ModuleBase *module) : CPElement(elem, module)
    {
        _label = elem->Get_Id();
    }
};

class PhiSeqCPElement : public SoloCPElement
{
protected:
    vcPhiSequencer* elem() { return (vcPhiSequencer*) _elem; }
public:
    void buildPN()
    {
        auto pnSampleAck = vce2pnnode(elem()->_phi_sample_ack);
        auto pnUpdAck = vce2pnnode(elem()->_phi_update_ack);
        auto pnMuxAck = vce2pnnode(elem()->_phi_mux_ack);

        auto pnSampleReq = vce2pnnode(elem()->_phi_sample_req);
        auto pnSampleReqPlace = pn()->createPlace("ps_sreqplace");
        pn()->annotatePNNode(pnSampleReqPlace, PassiveBranch_);
        pn()->createArc(pnSampleReq, pnSampleReqPlace);

        auto pnUpdateReq = vce2pnnode(elem()->_phi_update_req);
        auto pnUpdateReqPlace = pn()->createPlace("ps_ureqplace");
        pn()->annotatePNNode(pnUpdateReqPlace, PassiveBranch_);
        pn()->createArc(pnUpdateReq, pnUpdateReqPlace);

        // from architecture Behave of phi_sequencer_v2
        pn()->createArc(pnMuxAck, pnUpdAck);

        auto reduceSampleAck = pn()->createPlace("reduce_sacks");
        for(auto srcsack : elem()->_src_sample_acks)
            pn()->createArc(vce2pnnode(srcsack), reduceSampleAck);

        pn()->createArc(reduceSampleAck, pnSampleAck);

        // Extra tt Basis the way we handle Phi (Do we need these?)
        pn()->createArc(pnSampleReq, pnSampleAck);
        pn()->createArc(pnUpdateReq, pnUpdAck);
        pn()->createArc(pnSampleAck, pnUpdAck);

        for(int i=0; i<elem()->_triggers.size(); i++)
        {
            auto pnTrig = vce2pnnode(elem()->_triggers[i]);
            auto pnSrcUpdAck = vce2pnnode(elem()->_src_update_acks[i]);
            auto pnSrcSampleReq = vce2pnnode(elem()->_src_sample_reqs[i]);
            auto pnSrcUpdReq = vce2pnnode(elem()->_src_update_reqs[i]);
            auto pnPhiMuxReq = vce2pnnode(elem()->_phi_mux_reqs[i]);

            pn()->createArc(pnSrcUpdAck, pnPhiMuxReq);

            // conditional fork for phi_sample_req
            pn()->createArc(pnTrig, pnSrcSampleReq);
            pn()->createArc(pnSampleReqPlace, pnSrcSampleReq);
            // conditional fork for phi_update_req
            pn()->createArc(pnTrig, pnSrcUpdReq);
            pn()->createArc(pnUpdateReqPlace, pnSrcUpdReq);
        }
    }
    PhiSeqCPElement(vcPhiSequencer* elem, ModuleBase* module) : SoloCPElement(elem, module)
        { _pnnode = NULL; }
};

class LoopTerminatorCPElement : public SoloCPElement
{
    int _depth;
    vcCPElement *getUniqPred( vcCPElement* n )
    {
        auto preds = n->Get_Predecessors();
        if ( preds.size() != 1 )
        {
            cout << "getUniqPred expects unique pred, got " << preds.size() << " " << n->Kind() << ":" << n->Get_Id() << endl;
            exit(1);
        }
        return *preds.begin();
    }
    vcBranch* getLoopCondBranch()
    {
        auto le = elem()->Get_Loop_Exit();
        auto cdone = getUniqPred(le);
        auto cevaled = (vcTransition*) getUniqPred(cdone);
        auto cevaled_dpl = cevaled->Get_DP_Link();
        if ( cevaled_dpl.size() != 1 )
        {
            cout << "DP link size of condition_evaled transition expected to be 1, got " << cevaled_dpl.size() <<  " " << cevaled->Kind() << ":" << cevaled->Get_Id() << endl;
            exit(1);
        }
        return (vcBranch*) cevaled_dpl[0].first;
    }
protected:
    vcLoopTerminator* elem() { return (vcLoopTerminator*) _elem; }
public:
    void setdepth(int depth)
    {
        _depth = depth;
        ((PNPlace*) _pnnode)->setMarking(_depth-1);
        ((PNPlace*) _pnnode)->setCapacity(_depth);
    }
    bool isInfiniteLoop()
    {
        auto brplace = getLoopCondBranch();
        auto inpwires = brplace->Get_Input_Wires();
        if ( inpwires.size() != 1 )
        {
            cout << "Excpect inpwire count to be 1, got " << inpwires.size() << " " << brplace->Kind() << " " << brplace->Get_Id() << endl;
            exit(1);
        }
        // NOTE: When looping condition's input is constant we don't expect it to be 0
        return inpwires[0]->Is_Constant();
    }
    void buildPN()
    {

        auto depthPlace = (PNPlace*) _pnnode;

        // inputs
        auto pnLoopTerm = vce2pnnode(elem()->Get_Loop_Exit());
        auto pnLoopCont = vce2pnnode(elem()->Get_Loop_Taken());
        auto pnIterOver = vce2pnnode(elem()->Get_Loop_Body());

        // outputs
        auto pnLoopBack = vce2pnnode(elem()->Get_Loop_Back());
        auto pnLoopExit = (PNTransition*) vce2pnnode(elem()->Get_Exit_From_Loop());

        if ( isInfiniteLoop() )
        {
            pn()->createArc(pnIterOver,pnLoopBack);
            pn()->createArc(pnLoopTerm,pnLoopExit);
        }
        else
        {
            // We create output transitions internally and connect with the vC
            // created ones to avoid issues related to whether it is place or transition
            // On input side it seems to be always a transition
            auto exitOut = pn()->createTransition("exitOut");
            auto contOut = pn()->createTransition("contOut");

            // Our own intenal network
            pn()->createArc(pnLoopTerm, exitOut);
            pn()->createArc(pnLoopCont, contOut);
            pn()->createArc(pnIterOver, depthPlace);
            pn()->createArc(depthPlace, contOut);

            // Weighted arcs between depthPlace and exitOut
            pn()->createArc(depthPlace, exitOut, "", _depth);
            pn()->createArc(exitOut, depthPlace, "", _depth - 1);

            // from architecture Behave of loop_terminator
            // Connections between our transitions and outer elements
            pn()->createArc(exitOut, pnLoopExit);
            pn()->createArc(contOut, pnLoopBack);
        }
    }
    LoopTerminatorCPElement(vcLoopTerminator* elem, ModuleBase* module) : SoloCPElement(elem, module)
    {
        _pnnode = pn()->createPlace("MARKP:LoopDepth");
        pn()->annotatePNNode( _pnnode, PassiveBranch_ );
    }
};

class TransitionMergeCPElement : public SoloCPElement
{
protected:
    vcTransitionMerge* elem() { return (vcTransitionMerge*) _elem; }
public:
    void buildPN()
    {
        auto mypnnode = (PNPlace*) _pnnode;
        auto outpnnode = vce2pnnode(elem()->Get_Out_Transition());
        pn()->createArc(mypnnode, outpnnode);
        // from architecture default_arch of transition_merge
        // (just OrReduce)
        for(auto inTxn : elem()->Get_In_Transitions())
        {
            auto inpnnpde = vce2pnnode(inTxn);
            pn()->createArc(inpnnpde, mypnnode);
        }
    }
    TransitionMergeCPElement(vcTransitionMerge* elem, ModuleBase* module) : SoloCPElement(elem, module)
        { _pnnode = pn()->createPlace("TMRG:"+_label); }
};

#define MAPPER(VCTYP,TYP) \
    internmap<VCTYP*,TYP*> _##TYP \
        { internmap<VCTYP*,TYP*>(bind(&ElementFactory::create##TYP,this,_1)) }; \
    TYP* get##TYP(VCTYP *vce) { return _##TYP[vce]; }

#define CREATE(VCTYP,TYP) \
    TYP* create##TYP(VCTYP* vce) { return new TYP(vce,_module); }

class ElementFactory
{
public:
// create macros are used separately from MAPPER as some of them may need hand written logic
    MAPPER(vcDatapathElement,DPElement)
    CREATE(vcDatapathElement,DPElement)
    MAPPER(vcCPElementGroup,GroupCPElement)
    GroupCPElement* createGroupCPElement(vcCPElementGroup* vceg)
    {
        if ( vceg->Get_Is_Merge() and vceg->Get_Is_Fork() )
            return new PTElement(vceg,_module);
        else if ( vceg->Get_Is_Merge() )
            return new PlaceElement(vceg,_module);
        else
            return new TransElement(vceg,_module);
    }
    MAPPER(vcPhiSequencer,PhiSeqCPElement)
    CREATE(vcPhiSequencer,PhiSeqCPElement)
    MAPPER(vcTransitionMerge,TransitionMergeCPElement)
    CREATE(vcTransitionMerge,TransitionMergeCPElement)
    MAPPER(vcLoopTerminator,LoopTerminatorCPElement)
    CREATE(vcLoopTerminator,LoopTerminatorCPElement)
    ModuleBase* _module;
    ElementFactory(ModuleBase* module) : _module(module) {};
};

#define GETCPE(VCTYP,TYP) \
    CPElement* getCPE(VCTYP* vce) { return _ef.get##TYP(vce); }

class Module : public ModuleBase
{
    vcModule* _vcm;
    vcControlPath* _cp;
    SystemBase* _sys;
    map<string,DatumBase*> _inparamDatum;
    map<string,DatumBase*> _outparamDatum;
    list<CPElement*> _cpelist;
    list<DPElement*> _dpelist;
    function<void()> _exithook = NULL;
    PNPlace *_moduleEntryPlace, *_moduleExitPlace, *_moduleMutexOrDaemonPlace;
    ElementFactory _ef { ElementFactory(this) };
    bool _moduleExited = true;
    condition_variable _moduleExitedCV;
    mutex _moduleExitedMutex;
    map<string,unsigned> _inpParamPos;
    vector<Operator*> _ftlisteners;
public:
    bool _isDaemon;
    bool isDaemon() { return _isDaemon; }
    SystemBase* sys() { return _sys; }
    VcPetriNet* pn() { return _sys->pn(); }
    DPElement* getDPE(vcDatapathElement* vcdpe) { return _ef.getDPElement(vcdpe); }
    CPElement* getCPE(vcCPElement* cpe) { return _ef.getGroupCPElement(_cp->Get_Group(cpe)); }
    GETCPE(vcCPElementGroup,GroupCPElement)
    GETCPE(vcPhiSequencer,PhiSeqCPElement)
    GETCPE(vcLoopTerminator,LoopTerminatorCPElement)
    GETCPE(vcTransitionMerge,TransitionMergeCPElement)

    PNPlace* mutexPlace() { return _moduleMutexOrDaemonPlace; }
    PNPlace* entryPlace() { return _moduleEntryPlace; }
    PNTransition* entryTransition()
    {
        vcCPElement* entry = _cp->Get_Entry_Element();
        vcCPElementGroup* entryGroup = _cp->Get_CPElement_To_Group_Map()[entry];
        CPElement *entryCPE = getCPE(entryGroup);
        return (PNTransition*) entryCPE->inPNNode();
    }
    PNPlace* exitPlace() { return _moduleExitPlace; }
    bool isVolatile() { return _vcm->Get_Volatile_Flag(); }
    const list<DPElement*>& getDPEList() { return _dpelist; }
    DatumBase* opregForWire(vcWire *w)
    {
        vcDatapathElement *driver = w->Get_Driver();
        assert(driver);
        DPElement* driverDPE = getDPE(driver);
        vector<DatumBase*>& driveropv = driverDPE->opv();
        vector<int> opindices;
        // TODO: unclear why it is a vector, using [0] pending clarification
        driver->Get_Output_Wire_Indices(w, opindices);
        if ( opindices.size() != 1 )
        {
            cout << "DPE:Get_Output_Wire_Indices returns a vector of size !=1, unclear" << endl;
            exit(1);
        }
        int driveropindx = opindices[0];
        if ( driveropv.size() <= driveropindx )
        {
            cout << "When mapping wire to operator opreg, indx= " << driveropindx << " driveropv.size=" << driveropv.size() << " driverDPE: " << driverDPE->label() << endl;
            exit(1);
        }
        return driveropv[driveropindx];
    }
    string name() { return _vcm->Get_Id(); }
    DatumBase* inparamDatum(string param)
    {
        auto it = _inparamDatum.find(param);
        if ( it != _inparamDatum.end() ) return it->second;
        cout << "vc2pn: param not found in _inparamDatum " << param << endl;
        exit(1); 
    }
    DatumBase* opparamDatum(string param)
    {
        auto it = _outparamDatum.find(param);
        if ( it != _outparamDatum.end() ) return it->second;
        cout << "vc2pn: param not found in _outparamDatum " << param << endl;
        exit(1);
    }
    vector<DatumBase*> iparamV()
    {
        vector<DatumBase*> retv;
        for(auto arg:_vcm->Get_Ordered_Input_Arguments())
        {
            DatumBase* argdatum = inparamDatum(arg);
            retv.push_back(argdatum);
        }
        return retv;
    }
    vector<DatumBase*> oparamV()
    {
        vector<DatumBase*> retv;
        for(auto arg:_vcm->Get_Ordered_Output_Arguments())
        {
            DatumBase* argdatum = opparamDatum(arg);
            retv.push_back(argdatum);
        }
        return retv;
    }
    void printDPDotFile()
    {
        ofstream ofs;
        ofs.open(name()+"_dp.dot");
        _vcm->Get_Data_Path()->Print_Data_Path_As_Dot_File(ofs);
    }
    void moduleInvoke(const vector<DatumBase*>& inpv)
    {
        unique_lock<mutex> ul(_moduleExitedMutex);
        _moduleExited = false;
        invoke(inpv);
        _moduleExitedCV.wait( ul, [this] { return _moduleExited;} );
    }
    void invoke(const vector<DatumBase*>& inpv)
    {
        for(int i=0; i<inpv.size(); i++)
        {
            string arg = _vcm->Get_Ordered_Input_Arguments()[i];
            DatumBase* argdatum = inparamDatum(arg);
            argdatum->blindcopy(inpv[i]);
        }
        pn()->addtokens(_moduleEntryPlace, 1);
    }
    void registerFTListener(Operator *op) { _ftlisteners.push_back(op); }
    void moduleEntry()
    {
        cout << "Module " << name() << " invoked with:";
        for(auto ip:_inparamDatum)
            cout << " " << ip.first << "=" << ip.second->str();
        cout << endl;
        for( auto l : _ftlisteners ) l->flowthrough(0);
    }
    void moduleExit()
    {
        cout << "Module " << name() << " returning with:";
        for(auto op:_outparamDatum)
            cout << " " << op.first << "=" << op.second->str();
        cout << endl;
        {
            unique_lock<mutex> ul(_moduleExitedMutex);
            _moduleExited = true;
        }
        _moduleExitedCV.notify_all();
        if ( _exithook != NULL ) _exithook();
        else _exithook = NULL; // Once we exit, reset the hook, it is active only for 1 invocation
    }
    unsigned getInpParamPos(string paramname)
    {
        auto it = _inpParamPos.find(paramname);
        if ( it == _inpParamPos.end() )
        {
            cout << "vc2pn:getParamPos sought on non-existent param " << name() << " " << paramname << endl;
            exit(1);
        }
        return it->second;
    }
    void setInparamDatum()
    {
        for(auto iparam: _vcm->Get_Input_Arguments())
        {
            vcType* paramtyp = iparam.second->Get_Type();
            DatumBase* datum = _sys->vct2datum(paramtyp);
            _inparamDatum.emplace(iparam.first, datum);
        }
    }
    void setOutparamDatum()
    {
        for(auto oparam: _vcm->Get_Output_Arguments())
        {
            vcWire *owire = oparam.second;
            DatumBase *datum;
            if ( owire->Is_Constant() )
                datum = _sys->valueDatum( ((vcConstantWire*) owire)->Get_Value() );
            else
                datum = opregForWire(owire);
            _outparamDatum.emplace(oparam.first, datum);
        }
    }
    void buildPN()
    {
        for(auto e:_cpelist) e->buildPN();
        // Some buildPNs, such as Inport do PN trimming, which rquires cpelist to be processed before them
        for(auto e:_dpelist) e->buildPN();
        for(auto e:_dpelist) e->setinpv(); // Need to call this after buildPN of dpe, as datapath is cyclic


        vcCPElement* exitElem = _cp->Get_Exit_Element();
        vcCPElementGroup* exitGroup = _cp->Get_CPElement_To_Group_Map()[exitElem];
        CPElement *exitCPE = getCPE(exitGroup);

        // Sometimes exit node is a place, sometimes transition, uniformize it
        PNTransition *exitCPENode;
        if ( exitCPE->outPNNode()->typ() == PNElement::TRANSITION )
            exitCPENode = (PNTransition*) exitCPE->outPNNode();
        else
        {
            exitCPENode = pn()->createTransition("extraexit");
            pn()->createArc(exitCPE->outPNNode(), exitCPENode);
        }

        if ( not isVolatile() )
        {
            pn()->createArc(_moduleEntryPlace, entryTransition());
            pn()->createArc(exitCPENode, _moduleExitPlace );
        }

        if ( _isDaemon )
        {
            pn()->createArc(_moduleMutexOrDaemonPlace, _moduleEntryPlace);
            pn()->createArc(_moduleExitPlace, _moduleMutexOrDaemonPlace);
        }
    }
    void setExit(function<void()> exithook) { _exithook = exithook; }
    Module(vcModule* vcm, SystemBase* sys, bool isDaemon) : _vcm(vcm), _sys(sys), _isDaemon(isDaemon)
    {
        _cp = _vcm->Get_Control_Path();
        _moduleEntryPlace = pn()->createPlace("MOD:"+name()+".entry");
        _moduleExitPlace = pn()->createPlace("MOD:"+name()+".exit"); // Do not use DbgPlace for this, due to exit mechanism
        _moduleMutexOrDaemonPlace = pn()->createPlace("MARKP:" + name() + (_isDaemon ? ".daemon" : ".mutex"), 1 );
        if ( _vcm->Get_Num_Calls() > 1 )
        {
            pn()->annotatePNNode( _moduleMutexOrDaemonPlace, Mutex_ );
            pn()->annotatePNNode( _moduleExitPlace, PassiveBranch_ );
        }
        _moduleExitPlace->setAddActions(bind(&Module::moduleExit,this));
        _moduleEntryPlace->setAddActions(bind(&Module::moduleEntry,this));
        for(auto e:_cp->Get_CPElement_Groups())
            _cpelist.push_back(getCPE(e));
        for(auto slb:_cp->Get_Simple_Loop_Blocks())
        {
            auto loopTermCPE = (LoopTerminatorCPElement*) getCPE(slb->Get_Terminator());
            loopTermCPE->setdepth(slb->Get_Pipeline_Depth());
            _cpelist.push_back(loopTermCPE);
            auto plb = slb->Get_Loop_Body();
            for(auto psq : plb->Get_Phi_Sequencers()) _cpelist.push_back(getCPE(psq));
            for(auto tmerge : plb->Get_Transition_Merges()) _cpelist.push_back(getCPE(tmerge));
        }
        setInparamDatum(); // To be done before constructing _dpe as DPEs need input params
        for(auto dpet:_vcm->Get_Data_Path()->Get_DPE_Map())
            _dpelist.push_back(getDPE(dpet.second));
        setOutparamDatum();
        unsigned i=0;
        for(auto fp:_vcm->Get_Ordered_Input_Arguments()) _inpParamPos.emplace( fp, i++ );
    }
    ~Module()
    {
        for(auto e:_cpelist) delete e;
        for(auto e:_dpelist) delete e;
        for(auto d:_inparamDatum) delete d.second;
    }
};

// TODO: top module, trimming of modules, path reduction, overall optimizations
class System : public SystemBase
{
    map<string,Module*> _modules;
    map<vcPipe*,Pipe*> _pipemap;
    map<string,PipeFeeder*> _feedermap;
    map<string,PipeReader*> _readermap;
    map<vcStorageObject*,PNPlace*> _storageMutexPlaces;
    vcSystem* _vcs;
    VcPetriNet *_pn;
    PNPlace *_sysPreExitPlace;
    PNPTArc *_sysExitArc;
    OpFactory _opfactory = {this};
    const map<string, VCtyp> _vctypmap = VCTYPMAP;
#ifdef USECEP
    IntervalManager *_intervalManager;
#endif
    void markExit()
    {
        cout << "Received exit event" << endl;
        pn()->addtokens(_sysPreExitPlace, 1);
    }
    // system will exit after that many exit events occur, where an exit event
    // is when tokens are added to _sysPreExitPlace by calling markExit
    void setExitWt(unsigned wt) { _sysExitArc->_wt = wt; }
    void buildSysExitPN()
    {
        _sysPreExitPlace = pn()->createPlace("sysPreExit");

        auto pre2exitTransition = pn()->createTransition("sysPreToExit");

        pn()->createArc(_sysPreExitPlace, pre2exitTransition);
        _sysExitArc = (PNPTArc*) _sysPreExitPlace->_oarcs[0];

        auto sysExitPlace = pn()->createQuitPlace("sysExit");
        pn()->createArc(pre2exitTransition, sysExitPlace);

        // Only for those not passed to createArc, we have to add
        pn()->annotatePNNode( _sysPreExitPlace, SimuOnly_ );
        pn()->annotatePNNode( pre2exitTransition, SimuOnly_ );
        pn()->annotatePNNode( sysExitPlace, SimuOnly_ );
    }
    void buildPN()
    {
        for(auto m:_modules) m.second->buildPN();
        for(auto p:_pipemap) { p.second->buildPN(); }
        for(auto p:_feedermap) { p.second->buildPN(); }
        for(auto p:_readermap) { p.second->buildPN(); }
        buildSysExitPN();
#       ifdef SIMU_MODE_STPN
        pn()->setDelayModels();
#       endif
    }
public:
    string name() { return _vcs->Get_Id(); }
    map<string,vcPipe*> getPipeMap() { return _vcs->Get_Pipe_Map(); }
    void stop() { pn()->quit(); } // For low level simulator interface
    VcPetriNet* pn() { return _pn; }
    vcStorageObject* getStorageObj(vcLoadStore* dpe) { return _opfactory.getStorageObj(dpe); }
    map<vcStorageObject*,vector<DatumBase*>>& getStorageDatums() { return _opfactory.getStorageDatums(); }
    PNPlace* getStorageMutexPlace(vcDatapathElement *dpe)
    {
        auto sto = getStorageObj( (vcLoadStore*) dpe);
        auto it = _storageMutexPlaces.find(sto);
        if ( it == _storageMutexPlaces.end() )
        {
            auto storageMutex = pn()->createPlace("MARKP:" + sto->Get_Id() + ".mutex",1);
            pn()->annotatePNNode( storageMutex, Mutex_ );
            _storageMutexPlaces.emplace(sto, storageMutex);
            return storageMutex;
        }
        return it->second;
    }
    PipeFeeder* getFeeder(string pipename, bool noError = false)
    {
        auto it = _feedermap.find(pipename);
        if ( it == _feedermap.end() )
        {
            if ( noError ) return NULL;
            cout << "No such pipe : " << pipename << endl;
            exit(1);
        }
        return it->second;
    }
    PipeReader* getReader(string pipename, bool noError = false)
    {
        auto it = _readermap.find(pipename);
        if ( it == _readermap.end() )
        {
            if ( noError ) return NULL;
            cout << "No such pipe : " << pipename << endl;
            exit(1);
        }
        return it->second;
    }
    VCtyp vctyp(string Clsname)
    {
        auto it = _vctypmap.find(Clsname);
        if ( it == _vctypmap.end() )
        {
            cout << "opfactory: Unknown vC type " << Clsname << endl;
            exit(1);
        }
        return it->second;
    }
    Operator* createOperator(vcDatapathElement *dpe) { return _opfactory.dpe2op(dpe); }
    DatumBase* vct2datum(vcType* vct) { return _opfactory.vct2datum(vct); }
    DatumBase* valueDatum(vcValue* vcval) { return _opfactory.valueDatum(vcval); }
    ModuleBase* getModule(vcModule* vcm)
    {
        string modulename = vcm->Get_Id();
        auto it = _modules.find(modulename);
        if ( it != _modules.end() ) return it->second;
        cout << "vc2pn: Module not found " << modulename << endl;
        exit(1);
    }
    void printDPDotFiles() { for(auto m:_modules) m.second->printDPDotFile(); }
    void printPNDotFile() { _pn->printdot(); }
    void printPNPNMLFile() { _pn->printpnml(); }
    void wait() { _pn->wait(); }
    Pipe* pipeMap(vcPipe* pipe)
    {
        auto it = _pipemap.find(pipe);
        if ( it != _pipemap.end() ) return it->second;

        Pipe* retval = _opfactory.vcp2p(pipe, pn());
        _pipemap.emplace(pipe, retval);
        return retval;
    }
    void logSysOp(map<string,vector<DatumBase*>>& collectopmap)
    {
        for(auto c:collectopmap)
        {
            cout << "Collected from pipe " << c.first << endl;
            for(auto d:c.second) cout << "\t" << d->str() << endl;
        }
    }
    void fillCollectopmap(const vector<pair<string,unsigned>>& collects, map<string,vector<DatumBase*>>& collectopmap)
    {
        for(auto c:collects)
        {
            vector<DatumBase*>& v = _readermap[c.first]->collect();
            collectopmap.emplace(c.first,v);
        }
    }
    void moduleInvoke(string modulename, const vector<DatumBase*>& inpv)
    {
        auto it = _modules.find(modulename);
        if (it == _modules.end())
        {
            cout << "vc2pn:moduleInvoke Module not found: " << modulename << endl;
            exit(1);
        }
        else it->second->moduleInvoke(inpv);
    }
    // TODO: We may want to return module output vector and collection pipe vectors
    void invoke(string module, const vector<DatumBase*>& inpv,
        const vector<pair<string,vector<DatumBase*>>>& feeds,
        const vector<pair<string,unsigned>>& collects,
        map<string,vector<DatumBase*>>& collectopmap)
    {
        auto exitAction = bind(&System::markExit,this);
        bool haveModule = module != "";
        Module* calledModule;
        if ( haveModule )
        {
            auto it = _modules.find(module);
            if (it == _modules.end())
            {
                cout << "WARNING Module not found: " << module << endl;
                exit(1);
            }
            calledModule = it->second;
            if ( calledModule->_isDaemon )
            {
                cout << "Daemon module cannot be invoked from simulator. Pass empty string as module name to test daemons." << endl;
                exit(1);
            }
            calledModule->setExit(exitAction);
        }

        for(auto f:feeds)
            getFeeder(f.first)->feed(f.second);
        for(auto c:collects)
            getReader(c.first)->receive_async(c.second, exitAction);

        // Total exit event count is top module exit (if present) + completion
        // of all collects
        unsigned exitWt = haveModule + collects.size();
        if ( exitWt == 0 ) exitWt = 1; // min exit wt is 1
        setExitWt( exitWt );

        if ( haveModule ) calledModule->invoke(inpv);
        wait();
        fillCollectopmap(collects, collectopmap);
        logSysOp(collectopmap);
    }
    vector<DatumBase*> oparamV(string modulename)
    {
        auto it = _modules.find(modulename);
        if (it == _modules.end())
        {
            cout << "vc2pn:oparamV: Module not found: " << modulename << endl;
            exit(1);
        }
        else return it->second->oparamV();
    }
    System(vcSystem* vcs, const set<string>& daemons) : _vcs(vcs)
    {
        string basename = vcs->Get_Id();
#       ifdef USECEP
        _pn = new VcPetriNet ( basename, [this](unsigned e, unsigned long eseqno){ this->_intervalManager->route(e, eseqno); } );
#       else
        _pn = new VcPetriNet ( basename );
#       endif
        Pipe::setLogfile(basename+".pipes.log");
        Operator::setLogfile(basename+".ops.log");
        for(auto m:_vcs->Get_Ordered_Modules())
        {
            string modulename = m->Get_Id();
            bool isDaemon = daemons.find(modulename) != daemons.end();
            auto module = new Module(m, this, isDaemon);
            _modules.emplace(modulename, module);
        }

        for(auto pm:_pipemap)
        {
            vcPipe* vcp = pm.first;
            Pipe* p = pm.second;
            auto nReads = vcp->Get_Pipe_Read_Count();
            auto nWrites = vcp->Get_Pipe_Write_Count();
            bool isSysIn = nReads > 0 and nWrites == 0;
            bool isSysOut = nReads == 0 and nWrites > 0;
            string pname = vcp->Get_Id();
            if ( isSysIn )
            {
                auto feeder = new PipeFeeder(p);
                _feedermap.emplace(pname,feeder);
            }
            if ( isSysOut )
            {
                auto reader = new PipeReader(p);
                _readermap.emplace(pname,reader);
            }
        }
        buildPN();
#ifdef USECEP
        _intervalManager = new IntervalManager(basename, &_opfactory);
#endif
        _pn->init();
    }
    ~System()
    {
        for(auto m:_modules) delete m.second;
        _pn->deleteElems();
        delete _pn;
        for(auto p:_pipemap) delete p.second;
        for(auto p:_feedermap) delete p.second;
        for(auto p:_readermap) delete p.second;
#       ifdef USECEP
        delete _intervalManager;
#       endif
    }
};

#endif

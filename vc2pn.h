#ifndef _VC2PN_H
#define _VC2PN_H

#include <map>
#include <functional>
#include "vcLexer.hpp"
#include "vcParser.hpp"
#include "misc.h"
#include "dot.h"
#include "pninfo.h"
#include "operators.h"
#include "pipes.h"
#include "opf.h"
#include "vc2pnbase.h"
#include "opfactory.h"

#ifdef USECEP
#   include "intervals.h"
#endif

using namespace std::placeholders;

const string CEPDAT = "cepdat"; // Want to parameterize this file?

#define MAXINTWIDTH (sizeof(long)<<3)

#define PIPEWIDTH(PM,PT,CTYP) (Pipe*) new PM<CTYP,PT>(depth,width,pipe->Get_Id())
#define TRYCTYP(PM,PT,CTYP) width <= ( sizeof(CTYP) << 3 ) ? PIPEWIDTH(PM,PT,CTYP)
#define PIPETYP(PM,PT) TRYCTYP(PM,PT,unsigned char) : TRYCTYP(PM,PT,unsigned short) : TRYCTYP(PM,PT,unsigned int) : (Pipe*) new PM<unsigned long,PT>(depth,width,pipe->Get_Id())
#define PIPEMODE(PM) pipe->Get_Signal() ? PIPETYP(PM,SignalPipe) : pipe->Get_No_Block_Mode() ? PIPETYP(PM,NonBlockingPipe) : PIPETYP(PM,BlockingPipe)
#define PIPEINST pipe->Get_Lifo_Mode() ? PIPEMODE(Lifo) : PIPEMODE(Fifo);

typedef enum { int_, float_ } Wiretyp;

// Only essential of the vc world classes have a counterpart here as they carry
// substantial functionality. Some other classes such as vcModule, vcType,
// vcWire etc. do not have a counterpart here. Hence some operations on these
// classes do not have their own home, and hence are made into global
// functions.
Wiretyp vctWiretyp(vcType*);
unsigned vctDim(vcType*);
DatumBase* vct2datum(vcType*);
void vct2datums(vcType*, unsigned, vector<DatumBase*>&);
vector<Wiretyp> vcWiresTypes(vector<vcWire*>&);


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
    Element(vcRoot* elem, ModuleBase* module) : _elem(elem), _module(module) {}
    virtual void buildPN(PNInfo&)=0;
};

class DPElement : public Element
{
    const bool _isGuarded;
    vector<PNTransition*> _reqs, _acks;
    vector<PNTransition*> _greqs, _gacks;
    Operator *_op;
    BRANCH *_guardBranchOp;
    void indexcheck(int indx)
    {
        if ( (indx!=0) && (indx!=1) )
        {
            cout << "vc2pn: indx other than 0, 1 not handled (todo)" << endl;
            exit(1);
        }
    }
    void flowthrDrivers(vector<DPElement*>& ftdrvs)
    {
        for(auto w:elem()->Get_Input_Wires())
        {
            auto driver = w->Get_Driver();
            if (driver)
            {
                auto driverDPE = _module->getDPE(driver);
                if( driverDPE->isDeemedFlowThrough() )
                    ftdrvs.push_back( driverDPE );
            }
        }
    }
    void sreq2ack(PNInfo& pni) { buildSreqToAckPath(_reqs[0], _acks[0], pni); }
public:
    vcDatapathElement* elem() { return (vcDatapathElement*) _elem; }
    VCtyp _vctyp;
    // buildSreqToAckPath handles various flow through scenarios, hence its
    // name has become a misnomer and should be changed.
    // Note that strt, curend can be place or transition, further description
    // is assuming most common scenario of them being a transition.
    // Call begins with strt=sreq and curend=sack Go backward in flow-through
    // tree and connect sreq->curend if these is no flow through driver else
    // transitively insert all ftreq transitions in between and connect strt
    // with the leaf of the chain
    void buildSreqToAckPath(PNNode* strt, PNNode* curend, PNInfo& pni)
    {
        vector<DPElement*> ftdrvs;
        flowthrDrivers(ftdrvs);
        if ( ftdrvs.size() == 0 )
            PetriNet::createArc(strt, curend, pni.pnes);
        else for(auto ftdrv:ftdrvs)
        {
            auto ftreq = ftdrv->ftreq(pni);
            PetriNet::createArc(ftreq, curend, pni.pnes);
            ftdrv->buildSreqToAckPath(strt, ftreq, pni);
        }
    }
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
    void buildPNPhi(PNInfo& pni)
    {
        vector<DPElement*> ftdrvs;
        flowthrDrivers(ftdrvs);
        if( ftdrvs.size() != 0 )
        {
            // TODO: Check if phi can have ft drivers
            cout << "vc2pn: flowthrDrivers drivers for phi not handled" << endl;
            exit(1);
        }
        auto phplace = new PNPlace("DPE:" + _label + "_phplace");
        auto rootindex = elem()->Get_Root_Index();
        for(int i=0; i<_reqs.size(); i++)
        {
            auto req = _reqs[i];
            PetriNet::createArc(req, phplace, pni.pnes);
#           ifdef USECEP
            pni.vctid.add({ rootindex, "req" + to_string(i), req->_nodeid });
#           endif
            req->setEnabledActions(bind(&Operator::select,_op,i));
        }
        PetriNet::createArc(phplace, _acks[0], pni.pnes);
#       ifdef USECEP
        pni.vctid.add({ rootindex, "ack0", _acks[0]->_nodeid });
#       endif
        _acks[0]->setEnabledActions(bind(&Operator::uack,_op));

    }
    void buildPNBranch(PNInfo& pni)
    {
        vector<DPElement*> ftdrvs;
        flowthrDrivers(ftdrvs);
        if( ftdrvs.size() != 1 )
        {
            cout << "vc2pn: branch has flowthrough driver count != 1, unhandled" << endl;
            exit(1);
        }
        auto ftreq = ftdrvs[0]->ftreq(pni);
        auto brplace = new PNPlace("DPE:" + _label + "_brplace");
        brplace->setArcChooser(bind(&BRANCH::arcChooser,(BRANCH*)_op));
        // Unlike other users of buildSreqToAckPath, since we want it to end in
        // a place, we take first step ad hoc and ask the first driver to
        // invoke buildSreqToAckPath
        ftdrvs[0]->buildSreqToAckPath(_reqs[0], ftreq, pni);
        PetriNet::createArc(ftreq, brplace, pni.pnes);
        PetriNet::createArc(brplace, _acks[0], pni.pnes);
        PetriNet::createArc(brplace, _acks[1], pni.pnes );
        auto rootindex = elem()->Get_Root_Index();
#       ifdef USECEP
        pni.vctid.add({ rootindex, "req0", ftreq->_nodeid });
        pni.vctid.add({ rootindex, "ack0", _acks[0]->_nodeid });
        pni.vctid.add({ rootindex, "ack1", _acks[1]->_nodeid });
#       endif
    }
    int uackIndx()
    {
        return _vctyp == vcPhiPipelined_ ? 0 : 1;
    }
    bool isDeemedFlowThrough()
    {
        return elem()->Get_Flow_Through() or isSignalInport();
    }
    bool isDeemedGuarded()
    {
        return _isGuarded and not isDeemedFlowThrough();
    }
    bool isGuardCondFlowThrough()
    {
        if ( isDeemedGuarded() )
        {
            auto driver = elem()->Get_Guard_Wire()->Get_Driver();
            if ( driver == NULL ) return false;
            auto driverDPE = _module->getDPE(driver);
            return driverDPE->isDeemedFlowThrough();
        }
        else return false;
    }
    bool isSignalInport()
    {
        return _vctyp == vcInport_ and (((vcIOport*)elem())->Get_Pipe())->Get_Signal();
    }
    void wrapPNWithGuard(PNInfo& pni)
    {
        auto dpelabel = "DPE:" + _label + "_";
        PNTransition *go = new PNTransition(dpelabel + "go");
        PNTransition *nogo = new PNTransition(dpelabel + "nogo");
        PNTransition *nogo_ureq = new PNTransition(dpelabel + "nogo_ureq");
        PNPlace *brplace = new PNPlace(dpelabel + "brplace");
        brplace->setArcChooser(bind(&BRANCH::arcChooser, _guardBranchOp));
        PNPlace *gsackplace = new PNPlace(dpelabel + "gsackplace");
        PNPlace *guackplace = new PNPlace(dpelabel + "guackplace");
        PNPlace *gureqplace = new PNPlace(dpelabel + "gureqplace");

        auto gsreq = _greqs[0], gureq = _greqs[1], gsack = _gacks[0], guack = _gacks[1];
        auto sreq = _reqs[0], ureq = _reqs[1], sack = _acks[0], uack = _acks[1];

        if ( isGuardCondFlowThrough() )
        {
            auto driver = elem()->Get_Guard_Wire()->Get_Driver();
            assert(driver);
            auto driverDPE = _module->getDPE(driver);
            auto driverFtreq = driverDPE->ftreq(pni);
            driverDPE->buildSreqToAckPath(gsreq, driverFtreq, pni);
            PetriNet::createArc(driverFtreq, brplace, pni.pnes);
        }
        else
            PetriNet::createArc(gsreq, brplace, pni.pnes);

        if ( elem()->Get_Guard_Complement() )
        {
            PetriNet::createArc(brplace, go, pni.pnes);
            PetriNet::createArc(brplace, nogo, pni.pnes);
        }
        else
        {
            PetriNet::createArc(brplace, nogo, pni.pnes);
            PetriNet::createArc(brplace, go, pni.pnes);
        }
        PetriNet::createArc(nogo, gsackplace, pni.pnes);
        PetriNet::createArc(nogo, nogo_ureq, pni.pnes);
        PetriNet::createArc(nogo_ureq, guackplace, pni.pnes);
        PetriNet::createArc(go, sreq, pni.pnes);
        PetriNet::createArc(go, ureq, pni.pnes);
        PetriNet::createArc(sack, gsackplace, pni.pnes);
        PetriNet::createArc(uack, guackplace, pni.pnes);
        PetriNet::createArc(gsackplace, gsack, pni.pnes);
        PetriNet::createArc(guackplace, guack, pni.pnes);
        PetriNet::createArc(gureq, gureqplace, pni.pnes);
        PetriNet::createArc(gureqplace, ureq, pni.pnes);
        PetriNet::createArc(gureqplace, nogo_ureq, pni.pnes);
    }
    // buildPN that suits most DPEs
    void buildPNDefault(PNInfo& pni)
    {
        bool isIport = _vctyp == vcInport_;
        bool isOport = _vctyp == vcOutport_;
        if ( isIport ) // In AHIR Inport reads on ureq, not on sreq
            _reqs[1]->setEnabledActions(bind(&Operator::ureq,_op));
        else
            _acks[0]->setEnabledActions(bind(&Operator::sack,_op));
        _acks[1]->setEnabledActions(bind(&Operator::uack,_op));
        sreq2ack(pni);
        PetriNet::createArc(_acks[0], _acks[1], pni.pnes); // Since sreq/ureq can be ||, need this sync
        PetriNet::createArc(_reqs[1], _acks[1], pni.pnes);
        auto uack2sack = (PNPlace*) PetriNet::createArc(
            _acks[1], _acks[0], pni.pnes,
            "MARKP:" + _acks[0]->_name
            );
        uack2sack->setMarking(1);
        auto rootindex = elem()->Get_Root_Index();
#       ifdef USECEP
        pni.vctid.add({ rootindex, "req0", _reqs[0]->_nodeid });
        pni.vctid.add({ rootindex, "req1", _reqs[1]->_nodeid });
        pni.vctid.add({ rootindex, "ack0", _acks[0]->_nodeid });
        pni.vctid.add({ rootindex, "ack1", _acks[1]->_nodeid });
#       endif
        if ( isIport or isOport )
        {
            auto sreq = _reqs[0];
            auto sack = _acks[0];
            auto ureq = _reqs[1];
            auto uack = _acks[1];
            auto pipe = ((IOPort*)_op)->_pipe;
            if ( isIport ) pipe->buildPNIport(ureq, uack, pni); // See comment "In AHIR..." above
            else pipe->buildPNOport(sreq, sack, pni);
        }
        else if ( _vctyp == vcCall_ )
        {
            auto sack = _acks[0];
            auto uack = _acks[1];

            auto calledVcModule = ((vcCall*)_elem)->Get_Called_Module();
            auto calledModule = sys()->getModule( calledVcModule );
            auto calledExitPlace = calledModule->exitPlace();
            auto calledEntryPlace = calledModule->entryPlace();
            auto calledMutexPlace = calledModule->mutexPlace();

            auto inProgressPlace = new PNPlace(_label+".CallInProgress");
            // sack requires called module's mutex token
            PetriNet::createArc(calledMutexPlace, sack, pni.pnes);
            // and passes token to inProgressPlace and called module's entry
            PetriNet::createArc(sack, inProgressPlace, pni.pnes);
            PetriNet::createArc(sack, calledEntryPlace, pni.pnes);
            // uack requires inProgressPlace and called module's exit place token
            PetriNet::createArc(inProgressPlace, uack, pni.pnes);
            PetriNet::createArc(calledExitPlace, uack, pni.pnes);
            // and releases a token to called module's mutex
            PetriNet::createArc(uack, calledMutexPlace, pni.pnes);
        }
    }
    PNTransition* ftreq(PNInfo& pni)
    {
        PNTransition *ftreq = new PNTransition("DPE:" + _label + "_ftreq");
        ftreq->setEnabledActions(bind(&Operator::flowthrough,_op));
        // if this ftreq relates with a pipe, need to connect with pipe's pnet
        if ( isSignalInport() )
            ((IOPort*)_op)->_pipe->buildPNIport(ftreq, ftreq, pni);
        return ftreq;
    }
    void createGuardReqs()
    {
        for(int i=0; i<elem()->Get_Number_Of_Reqs(); i++)
            _greqs.push_back(new PNTransition("DPE:" + _label + "_greq_" + to_string(i)));
    }
    void createGuardAcks()
    {
        for(int i=0; i<elem()->Get_Number_Of_Reqs(); i++)
            _gacks.push_back(new PNTransition("DPE:" + _label + "_gack_" + to_string(i)));
    }
    void createReqs()
    {
        // For Flow Through elements reqs are created on demand so that for
        // each context a separate req is created
        if ( not elem()->Get_Flow_Through() )
            for(int i=0; i<elem()->Get_Number_Of_Reqs(); i++)
                _reqs.push_back(new PNTransition("DPE:" + _label + "_req_" + to_string(i)));
    }
    void createAcks()
    {
        for(int i=0; i<elem()->Get_Number_Of_Acks(); i++)
            _acks.push_back(new PNTransition("DPE:" + _label + "_ack_" + to_string(i)));
    }
    void buildPN(PNInfo& pni)
    {
        if ( isDeemedFlowThrough() ); // PN built by consumers / module output for flow throughs
        else if ( _vctyp == vcBranch_ )
            buildPNBranch(pni);
        else if ( _vctyp == vcPhi_ or _vctyp == vcPhiPipelined_ )
            buildPNPhi(pni);
        else if ( ( _reqs.size() == 2 ) && ( _acks.size() == 2 ) ) // Default handling
            buildPNDefault(pni);
        else
        {
            cout << "vc2pn: unhandled req-ack pattern reqs=" << _reqs.size() << " acks=" << _acks.size() << " " << _elem->Kind() << " " << endl;
            exit(1);
        }
        if ( isDeemedGuarded() ) wrapPNWithGuard(pni);
    }

    vector<Wiretyp> inptyps() { return vcWiresTypes(elem()->Get_Input_Wires()); }
    vector<Wiretyp> optyps() { return vcWiresTypes(elem()->Get_Output_Wires()); }
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
    void setinpv()
    {
        vector<DatumBase*> inpv;
        for( auto w : elem()->Get_Input_Wires() )
        {
            DatumBase* inpdatum;
            if ( w->Kind() == "vcInputWire" )
                inpdatum = _module->inparamDatum( w->Get_Id() );
            else if ( w->Is_Constant() )
                inpdatum = sys()->valueDatum( ((vcConstantWire*) w)->Get_Value() );
            else
                inpdatum = _module->opregForWire(w);
            inpv.push_back(inpdatum);
        }
        _op->setinpv(inpv);

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
        createReqs();
        createAcks();
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
    virtual void buildPNlocal(PNInfo& pni)
    {
    }
public:
    void buildPN(PNInfo& pni)
    {
        buildPNlocal(pni);
        // TODO: Take input and output transition first, then other things
        // from transition take its DPE and index, from that DPE and index take corresp transition from DPE
        if ( elem()->Get_Has_Input_Transition() )
        {
            vcTransition *inpt = elem()->Get_Input_Transition();
            vcDatapathElement* vcdpe = vct2vcdpe(inpt);
            DPElement *dpe = _module->getDPE(vcdpe);
            int ackindx = vcdpe->Get_Ack_Index(inpt);
            PNTransition *inppnt = dpe->getAckTransition(ackindx);
            PetriNet::createArc(inppnt, inPNNode(), pni.pnes);
        }
        else
        {
            for(auto predeg: elem()->Get_Predecessors())
            {
                auto *predcpe = (GroupCPElement*) _module->getCPE(predeg);
                PetriNet::createArc(predcpe->outPNNode(), inPNNode(), pni.pnes);
            }
            // TODO: Temporarily replicating _predecessor logic for _marked_predecessors
            for(auto predeg: elem()->Get_Marked_Predecessors())
            {
                auto *predcpe = (GroupCPElement*) _module->getCPE(predeg);
                auto markedplace = (PNPlace*) PetriNet::createArc(
                    predcpe->outPNNode(), inPNNode(), pni.pnes,
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
                PetriNet::createArc(outPNNode(), optrant, pni.pnes);
            }
        }
        // NOTE: _pnnode serves as output end, hence below code won't work for
        // some nodes (e.g. those with fork and merge combined)
        //
        //else
        //    for(auto succeg: _elem->_successors)
        //    {
        //        CPElement *succcpe = _module->getCPE(succeg);
        //        PetriNet::createArc(_pnnode, succcpe->_pnnode, pnes);
        //    }
        //if ( _elem->_is_join or _elem->Get_Is_Merge() )
        //{
        //    for(auto predeg: _elem->Get_Predecessors())
        //    {
        //        CPElement *predcpe = _module->getCPE(predeg);
        //        PetriNet::createArc(predcpe->_pnnode, _pnnode, pnes);
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
        _pnnode = new PNPlace("CPE:"+to_string(elem()->Get_Group_Index())+":"+label());
    }
};

class TransElement : public GroupCPElement
{
public:
    string shape() { return "rectangle"; }
    string color() { return "blue"; }
    TransElement(vcCPElementGroup* elemg, ModuleBase* module) : GroupCPElement(elemg, module)
    {
        _pnnode = new PNTransition("CPE:"+to_string(elem()->Get_Group_Index())+":"+label());
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
    void buildPNlocal(PNInfo& pni)
    {
        PetriNet::createArc(_inPNNode, _outPNNode, pni.pnes);
    }
    PTElement(vcCPElementGroup* elemg, ModuleBase* module) : GroupCPElement(elemg, module)
    {
        _pnnode = NULL;
        _inPNNode = new PNPlace("CPE:P:"+to_string(elem()->Get_Group_Index())+":"+label());
        _outPNNode = new PNTransition("CPE:T:"+to_string(elem()->Get_Group_Index())+":"+label());
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
    void buildPN(PNInfo& pni)
    {
        auto pnSampleAck = vce2pnnode(elem()->_phi_sample_ack);
        auto pnUpdAck = vce2pnnode(elem()->_phi_update_ack);
        auto pnMuxAck = vce2pnnode(elem()->_phi_mux_ack);

        auto pnSampleReq = vce2pnnode(elem()->_phi_sample_req);
        auto pnSampleReqPlace = new PNPlace("ps_sreqplace");
        PetriNet::createArc(pnSampleReq, pnSampleReqPlace, pni.pnes);

        auto pnUpdateReq = vce2pnnode(elem()->_phi_update_req);
        auto pnUpdateReqPlace = new PNPlace("ps_ureqplace");
        PetriNet::createArc(pnUpdateReq, pnUpdateReqPlace, pni.pnes);

        // from architecture Behave of phi_sequencer_v2
        PetriNet::createArc(pnMuxAck, pnUpdAck, pni.pnes);

        auto reduceSampleAck = new PNPlace("reduce_sacks");
        for(auto srcsack : elem()->_src_sample_acks)
            PetriNet::createArc(vce2pnnode(srcsack), reduceSampleAck, pni.pnes);

        PetriNet::createArc(reduceSampleAck, pnSampleAck, pni.pnes);

        // Extra tt Basis the way we handle Phi (Do we need these?)
        PetriNet::createArc(pnSampleReq, pnSampleAck, pni.pnes);
        PetriNet::createArc(pnUpdateReq, pnUpdAck, pni.pnes);
        PetriNet::createArc(pnSampleAck, pnUpdAck, pni.pnes);

        for(int i=0; i<elem()->_triggers.size(); i++)
        {
            auto pnTrig = vce2pnnode(elem()->_triggers[i]);
            auto pnSrcUpdAck = vce2pnnode(elem()->_src_update_acks[i]);
            auto pnSrcSampleReq = vce2pnnode(elem()->_src_sample_reqs[i]);
            auto pnSrcUpdReq = vce2pnnode(elem()->_src_update_reqs[i]);
            auto pnPhiMuxReq = vce2pnnode(elem()->_phi_mux_reqs[i]);

            PetriNet::createArc(pnSrcUpdAck, pnPhiMuxReq, pni.pnes);

            // conditional fork for phi_sample_req
            PetriNet::createArc(pnTrig, pnSrcSampleReq, pni.pnes);
            PetriNet::createArc(pnSampleReqPlace, pnSrcSampleReq, pni.pnes);
            // conditional fork for phi_update_req
            PetriNet::createArc(pnTrig, pnSrcUpdReq, pni.pnes);
            PetriNet::createArc(pnUpdateReqPlace, pnSrcUpdReq, pni.pnes);
        }
    }
    PhiSeqCPElement(vcPhiSequencer* elem, ModuleBase* module) : SoloCPElement(elem, module)
        { _pnnode = NULL; }
};

class LoopTerminatorCPElement : public SoloCPElement
{
    int _depth;
protected:
    vcLoopTerminator* elem() { return (vcLoopTerminator*) _elem; }
public:
    void setdepth(int depth)
    {
        _depth = depth;
        ((PNPlace*) _pnnode)->setMarking(_depth-1);
    }
    void buildPN(PNInfo& pni)
    {
        auto depthPlace = (PNPlace*) _pnnode;

        // inputs
        auto pnLoopTerm = vce2pnnode(elem()->Get_Loop_Exit());
        auto pnLoopCont = vce2pnnode(elem()->Get_Loop_Taken());
        auto pnIterOver = vce2pnnode(elem()->Get_Loop_Body());

        // outputs
        auto pnLoopBack = vce2pnnode(elem()->Get_Loop_Back());
        auto pnLoopExit = (PNTransition*) vce2pnnode(elem()->Get_Exit_From_Loop());

        // We create output transitions internally and connect with the vC
        // created ones to avoid issues related to whether it is place or transition
        // On input side it seems to be always a transition
        auto exitOut = new PNTransition("exitOut");
        auto contOut = new PNTransition("contOut");

        // Our own intenal network
        PetriNet::createArc(pnLoopTerm, exitOut, pni.pnes);
        PetriNet::createArc(pnLoopCont, contOut, pni.pnes);
        PetriNet::createArc(pnIterOver, depthPlace, pni.pnes);
        PetriNet::createArc(depthPlace, contOut, pni.pnes);

        // Weighted arcs between depthPlace and exitOut
        auto depthExitArc = new PNPTArc(depthPlace, exitOut);
        depthExitArc->_wt = _depth;
        auto exitDepthArc = new PNTPArc(exitOut, depthPlace);
        exitDepthArc->_wt = _depth - 1;
        // Since we do not use createArc (because we want access to arc wt) have to insert them all
        pni.pnes.insert(depthPlace);
        pni.pnes.insert(exitOut);
        pni.pnes.insert(depthExitArc);
        pni.pnes.insert(exitDepthArc);

        // from architecture Behave of loop_terminator
        // Connections between our transitions and outer elements
        PetriNet::createArc(exitOut, pnLoopExit, pni.pnes);
        PetriNet::createArc(contOut, pnLoopBack, pni.pnes);

    }
    LoopTerminatorCPElement(vcLoopTerminator* elem, ModuleBase* module) : SoloCPElement(elem, module)
        { _pnnode = new PNPlace("MARKP:LoopDepth"); }
};

class TransitionMergeCPElement : public SoloCPElement
{
protected:
    vcTransitionMerge* elem() { return (vcTransitionMerge*) _elem; }
public:
    void buildPN(PNInfo& pni)
    {
        auto mypnnode = (PNPlace*) _pnnode;
        auto outpnnode = vce2pnnode(elem()->Get_Out_Transition());
        PetriNet::createArc(mypnnode, outpnnode, pni.pnes);
        // from architecture default_arch of transition_merge
        // (just OrReduce)
        for(auto inTxn : elem()->Get_In_Transitions())
        {
            auto inpnnpde = vce2pnnode(inTxn);
            PetriNet::createArc(inpnnpde, mypnnode, pni.pnes);
        }
    }
    TransitionMergeCPElement(vcTransitionMerge* elem, ModuleBase* module) : SoloCPElement(elem, module)
        { _pnnode = new PNPlace("TMRG:"+_label); }
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
public:
    bool _isDaemon;
    SystemBase* sys() { return _sys; }
    DPElement* getDPE(vcDatapathElement* vcdpe) { return _ef.getDPElement(vcdpe); }
    CPElement* getCPE(vcCPElement* cpe) { return _ef.getGroupCPElement(_cp->Get_Group(cpe)); }
    GETCPE(vcCPElementGroup,GroupCPElement)
    GETCPE(vcPhiSequencer,PhiSeqCPElement)
    GETCPE(vcLoopTerminator,LoopTerminatorCPElement)
    GETCPE(vcTransitionMerge,TransitionMergeCPElement)

    PNPlace* mutexPlace() { return _moduleMutexOrDaemonPlace; }
    PNPlace* entryPlace() { return _moduleEntryPlace; }
    PNPlace* exitPlace() { return _moduleExitPlace; }
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
    void invoke(const vector<DatumBase*>& inpv)
    {
        for(int i=0; i<inpv.size(); i++)
        {
            string arg = _vcm->Get_Ordered_Input_Arguments()[i];
            DatumBase* argdatum = inparamDatum(arg);
            *argdatum = inpv[i];
        }
        _moduleEntryPlace->addtokens(1);
    }
    void moduleEntry()
    {
        cout << "Module " << name() << " invoked with:";
        for(auto ip:_inparamDatum)
            cout << " " << ip.first << "=" << ip.second->str();
        cout << endl;
    }
    void moduleExit()
    {
        cout << "Module " << name() << " returning with:";
        for(auto op:_outparamDatum)
            cout << " " << op.first << "=" << op.second->str();
        cout << endl;
        if ( _exithook != NULL ) _exithook();
        else _exithook = NULL; // Once we exit, reset the hook, it is active only for 1 invocation
    }
    void setInparamDatum()
    {
        for(auto iparam: _vcm->Get_Input_Arguments())
        {
            vcType* paramtyp = iparam.second->Get_Type();
            DatumBase* datum = vct2datum(paramtyp);
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
    void flowthrOpDrivers(vector<DPElement*>& ftopdrvs)
    {
        for(auto oparam: _vcm->Get_Output_Arguments())
        {
            auto *owire = oparam.second;
            auto driver = owire->Get_Driver();
            if ( driver )
            {
                auto driverDPE = getDPE(driver);
                if( driverDPE->isDeemedFlowThrough() )
                    ftopdrvs.push_back( driverDPE );
            }
        }
    }
    void buildPN(PNInfo& pni)
    {
        for(auto e:_dpelist) e->buildPN(pni);
        for(auto e:_dpelist) e->setinpv(); // Need to call this after buildPN of dpe, as datapath is cyclic
        for(auto e:_cpelist) e->buildPN(pni);

        vcCPElement* entry = _cp->Get_Entry_Element();
        vcCPElementGroup* entryGroup = _cp->Get_CPElement_To_Group_Map()[entry];
        CPElement *entryCPE = getCPE(entryGroup);
        PetriNet::createArc(_moduleEntryPlace, entryCPE->inPNNode(), pni.pnes);

        vcCPElement* exitElem = _cp->Get_Exit_Element();
        vcCPElementGroup* exitGroup = _cp->Get_CPElement_To_Group_Map()[exitElem];
        CPElement *exitCPE = getCPE(exitGroup);

        // Sometimes exit node is a place, sometimes transition, uniformize it
        PNTransition *exitCPENode;
        if ( exitCPE->outPNNode()->typ() == PNElement::TRANSITION )
            exitCPENode = (PNTransition*) exitCPE->outPNNode();
        else
        {
            exitCPENode = new PNTransition("extraexit");
            PetriNet::createArc(exitCPE->outPNNode(), exitCPENode, pni.pnes);
        }

        vector<DPElement*> ftopdrvs;
        flowthrOpDrivers(ftopdrvs);

        // If some flow through operators connect to outputs trigger their ft
        // transition between exitCPENode and _moduleExitPlace
        if ( ftopdrvs.size() == 0 )
            PetriNet::createArc(exitCPENode, _moduleExitPlace, pni.pnes);
        else
        {
            PNTransition *exitCPENode1 = new PNTransition("ftcollect");
            for(auto ftopdrv:ftopdrvs)
            {
                auto ftr = ftopdrv->ftreq(pni);
                ftopdrv->buildSreqToAckPath(exitCPENode, ftr, pni);
                PetriNet::createArc(ftr, exitCPENode1, pni.pnes);
            }
            PetriNet::createArc(exitCPENode1, _moduleExitPlace, pni.pnes);
        }

        if ( _isDaemon )
        {
            PetriNet::createArc(_moduleMutexOrDaemonPlace, _moduleEntryPlace, pni.pnes);
            PetriNet::createArc(_moduleExitPlace, _moduleMutexOrDaemonPlace, pni.pnes);
        }
        else pni.pnes.insert(_moduleMutexOrDaemonPlace); // For top modules no createArc, so let's add

    }
    void setExit(function<void()> exithook) { _exithook = exithook; }
    Module(vcModule* vcm, SystemBase* sys, bool isDaemon) : _vcm(vcm), _sys(sys), _isDaemon(isDaemon)
    {
        _cp = _vcm->Get_Control_Path();
        _moduleEntryPlace = new PNPlace("MOD:"+name()+".entry");
        _moduleExitPlace = new PNPlace("MOD:"+name()+".exit"); // Do not use DbgPlace for this, due to exit mechanism
        _moduleMutexOrDaemonPlace = new PNPlace("MARKP:" + name() + (_isDaemon ? ".daemon" : ".mutex"), 1 );
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
    map<vcValue*,DatumBase*> _valueDatum;
    map<vcStorageObject*,vector<DatumBase*>> _storageDatums;
    map<vcPipe*,Pipe*> _pipemap;
    map<string,PipeFeeder*> _feedermap;
    map<string,PipeReader*> _readermap;
    vcSystem* _vcs;
    PetriNet *_pn;
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
        _sysPreExitPlace->addtokens(1);
    }
    // system will exit after that many exit events occur, where an exit event
    // is when tokens are added to _sysPreExitPlace by calling markExit
    void setExitWt(unsigned wt) { _sysExitArc->_wt = wt; }
    void buildSysExitPN(PNInfo& pni)
    {
        _sysPreExitPlace = new PNPlace("sysPreExit");

        auto pre2exitTransition = new PNTransition("sysPreToExit");

        _sysExitArc = new PNPTArc(_sysPreExitPlace, pre2exitTransition);

        auto sysExitPlace = new PNQuitPlace("sysExit");
        PetriNet::createArc(pre2exitTransition, sysExitPlace, pni.pnes);

        // Only for those not passed to createArc, we have to add
        pni.pnes.insert(_sysPreExitPlace);
        pni.pnes.insert(_sysExitArc);
    }
    void buildPN()
    {
        for(auto m:_modules) m.second->buildPN(_pni);
        for(auto p:_pipemap) { p.second->buildPN(_pni); }
        for(auto p:_feedermap) { p.second->buildPN(_pni); }
        for(auto p:_readermap) { p.second->buildPN(_pni); }
        buildSysExitPN(_pni);
#ifdef USECEP
        _intervalManager = new IntervalManager(CEPDAT);
        _pn = new PetriNet( _pni.pnes, [this](unsigned e){ this->_intervalManager->route(e); } );
#else
        _pn = new PetriNet( _pni.pnes );
#endif
    }
public:
    PNInfo _pni;
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
    void printPNJsonFile() { _pn->printjson(); }
    void printPNPNMLFile() { _pn->printpnml(); }
    void wait() { _pn->wait(); }
    PetriNet* pn() { return _pn; }
    DatumBase* valueDatum(vcValue* vcval)
    {
        auto it = _valueDatum.find(vcval);
        if ( it != _valueDatum.end() ) return it->second;

        vcType* vctyp = vcval->Get_Type();
        DatumBase* retdat = vct2datum(vctyp);
        string val = ((vcIntValue*)vcval)->Get_Value();
        *retdat = val;
        _valueDatum.emplace(vcval, retdat);
        return retdat;
    }
    vector<DatumBase*>& storageDatums(vcStorageObject* sto)
    {
        auto it = _storageDatums.find(sto);
        if ( it != _storageDatums.end() ) return it->second;

        auto stoTyp = sto->Get_Type();
        auto wtyp = vctWiretyp(stoTyp);
        auto dim = vctDim(stoTyp);
        vector<DatumBase*> retdat;
        vct2datums(stoTyp, dim, retdat);
        _storageDatums.emplace(sto, retdat);
        return _storageDatums[sto];
    }
    Pipe* pipeMap(vcPipe* pipe)
    {
        auto it = _pipemap.find(pipe);
        if ( it != _pipemap.end() ) return it->second;

        int depth = pipe->Get_Depth();
        int width = pipe->Get_Width();
        if ( width > MAXINTWIDTH )
        {
            cout << "Pipes of width > " << MAXINTWIDTH << " not supported, got " << width << endl;
            exit(1);
        }
        Pipe* retval = PIPEINST

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
        {
            auto it = _feedermap.find(f.first);
            if ( it == _feedermap.end() )
            {
                cout << "No such pipe : " << f.first << endl;
                exit(1);
            }
            it->second->feed(f.second);
        }

        for(auto c:collects)
        {
            auto it = _readermap.find(c.first);
            if ( it == _readermap.end() )
            {
                cout << "No such pipe : " << c.first << endl;
                exit(1);
            }
            it->second->receive(c.second, exitAction);
        }

        // Total exit event count is top module exit (if present) + completion of all collects
        setExitWt( haveModule + collects.size() );

        if ( haveModule ) calledModule->invoke(inpv);
        wait();
        fillCollectopmap(collects, collectopmap);
        logSysOp(collectopmap);
    }
    System(vcSystem* vcs, const set<string>& daemons) : _vcs(vcs)
    {
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
            string pname = vcp->Get_Id();
            auto feeder = new PipeFeeder(p);
            auto reader = new PipeReader(p);
            _feedermap.emplace(pname,feeder);
            _readermap.emplace(pname,reader);
        }
        buildPN();
        _pn->init();
    }
    ~System()
    {
        for(auto m:_modules) delete m.second;
        _pn->deleteElems();
        delete _pn;
        for(auto d:_valueDatum) delete d.second;
        for(auto dv:_storageDatums) for(auto d:dv.second) delete d;
        for(auto p:_pipemap) delete p.second;
        for(auto p:_feedermap) delete p.second;
        for(auto p:_readermap) delete p.second;
#ifdef USECEP
        delete _intervalManager;
#endif
    }
};

#endif

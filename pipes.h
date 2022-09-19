#ifndef _PIPES_H
#define _PIPES_H

#include <iostream>
#include <vector>
#include <queue>
#include "datum.h"
#include "pninfo.h"

#ifdef PIPEDBG
    static ofstream plog("pipes.log");
    static mutex plogmutex;
#   define PIPELOG(ARGS) \
        plogmutex.lock(); \
        plog << ARGS << endl; \
        plogmutex.unlock();
#else
#   define PIPELOG(ARGS)
#endif

// (Unsynchronized) counter initialized to 0 and counts [0,modulo-1] with ++
// and -- operators (prefix and postfix)
class ModCntr
{
    unsigned _maxval;
    unsigned _val = 0;
    void inc() { _val = next(); }
    void dec() { _val = prev(); }
public:
    unsigned val() { return _val; }
    unsigned prev() { return _val ? _val - 1 : _maxval; }
    unsigned next() { return _val == _maxval ? 0 : _val + 1; }
    unsigned operator ++() { inc(); return _val; }
    unsigned operator --() { dec(); return _val; }
    unsigned operator ++(int) { unsigned retval = _val; inc(); return retval; }
    unsigned operator --(int) { unsigned retval = _val; dec(); return retval; }
    bool operator ==(ModCntr& rval) { return _val == rval._val; }
    ModCntr(unsigned modulo) : _maxval(modulo-1) {}
};

class Pipe
{
protected:
    unsigned _depth;
    unsigned _nElems = 0;
    vector<DatumBase*> _store;
    DatumBase* _zero; // Some subclasses need this
    ModCntr _wpos;
    unsigned pushpos() { return _wpos++; }
    virtual unsigned poppos()=0;
    virtual unsigned lastPopPosOnEmpty()=0;
    PNPlace *_mutexPlace;
    void buildPNMutexDep(PNTransition *req, PNTransition *ack, PNInfo& pni)
    {
        // for i or o, req should seek token from mutexplace
        PetriNet::createArc(_mutexPlace, req, pni.pnes);
        // for i or o, ack should release token to mutexplace
        PetriNet::createArc(ack, _mutexPlace, pni.pnes);
    }
    template <typename T> void initStore(unsigned depth, unsigned width)
    {
        auto zero = new Datum<T>(width);
        *zero = (T) 0;
        _zero = zero;
        for(int i=0; i<depth; i++)
        {
            auto datum = new Datum<T>(width);
            // initialization matters to signal only
            *datum = (T) 0;
            _store.push_back( datum );
        }
    }
    virtual void pushOnFull()=0;
    virtual DatumBase* popOnEmpty()=0;
    // pipe sub-type specific petri net is indicated by suffix 1
    virtual void buildPNOport1(PNTransition *sreq, PNTransition *sack, PNInfo& pni)=0;
    virtual void buildPNIport1(PNTransition *ureq, PNTransition *uack, PNInfo& pni)=0;
public:
    string _label;
    bool empty() { return _nElems == 0; }
    bool full() { return _nElems == _depth; }
    void push(DatumBase* din)
    {
        if ( full() )
        {
            PIPELOG("pushfull:" << _label << ":" << din->str())
            pushOnFull();
        }
        else
        {
            unsigned i = pushpos();
            _nElems++;
            _store[i]->blindcopy(din);
            PIPELOG("push:" << _label << ":" << din->str())
        }
    }
    DatumBase* pop()
    {
        if ( empty() )
        {
            auto retval = popOnEmpty();
            PIPELOG("popempty:" << _label << ":" << retval->str())
            return retval;
        }
        else
        {
            unsigned i = poppos();
            _nElems--;
            PIPELOG("pop:" << _label << ":" << _store[i]->str())
            return _store[i];
        }
    }
    // Note that buildPN is for part of pipe's own network and buildPN[IO]port
    // are for its connections with pipe's users requests and acks
    // So buildPN is called once per pipe and buildPN[IO]port once per usage
    // instance of that pipe
    virtual void buildPN(PNInfo& pni) {}
    void buildPNIport(PNTransition *ureq, PNTransition *uack, PNInfo& pni)
    {
        buildPNMutexDep(ureq, uack, pni);
        buildPNIport1(ureq, uack, pni);
    }
    void buildPNOport(PNTransition *sreq, PNTransition *sack, PNInfo& pni)
    {
        buildPNMutexDep(sreq, sack, pni);
        buildPNOport1(sreq, sack, pni);
    }
    Pipe(unsigned depth, string label) : _depth(depth), _wpos(ModCntr(depth)), _label(label)
    {
        _mutexPlace = new PNPlace("MARKP:"+_label+".Mutex",1);
    }
    ~Pipe()
    {
        for(auto d:_store) delete d;
        delete _zero;
    }
};

class BlockingPipe : public Pipe
{
protected:
    PNPlace *_freePlace, *_filledPlace;
    void pushOnFull()
    {
        cout << "Attempt to push to full blocking pipe (internal error)" << endl;
        exit(1);
    }
    DatumBase* popOnEmpty()
    {
        cout << "Attempt to pop from empty blocking pipe (internal error)" << endl;
        exit(1);
    }
    void buildPNOport1(PNTransition *sreq, PNTransition *sack, PNInfo& pni)
    {
        // out port sreq should seek token from freePlace
        PetriNet::createArc(_freePlace, sreq, pni.pnes);
        // out port sack should release a token to filledPlace
        PetriNet::createArc(sreq, _filledPlace, pni.pnes);
    }
    void buildPNIport1(PNTransition *ureq, PNTransition *uack, PNInfo& pni)
    {
        // in port ureq should need a token in filledPlace
        PetriNet::createArc(_filledPlace, ureq, pni.pnes);
        // in port should release a token to freePlace on uack
        PetriNet::createArc(uack, _freePlace, pni.pnes);
    }
    BlockingPipe(unsigned depth, string label) : Pipe(depth,label)
    {
        _freePlace = new PNPlace("MARKP:"+_label+".Depth",depth);
        _filledPlace = new PNPlace(_label+".Filled");
    }
};

class NonBlockingPipe : public BlockingPipe
{
    PNPlace *_popPlace;
protected:
    DatumBase* popOnEmpty() { return _zero; }
    void buildPNIport1(PNTransition *ureq, PNTransition *uack, PNInfo& pni)
    {
        PetriNet::createArc(ureq, _popPlace, pni.pnes);
    }
public:
    void buildPN(PNInfo& pni)
    {
        auto popEmpty = new PNTransition(_label+".popEmpty");
        auto popNonEmpty = new PNTransition(_label+".popNonEmpty");
        PetriNet::createArc(_popPlace, popEmpty, pni.pnes);
        PetriNet::createArc(_popPlace, popNonEmpty, pni.pnes);
        PetriNet::createArc(_filledPlace, popNonEmpty, pni.pnes);
        PetriNet::createArc(popNonEmpty, _freePlace, pni.pnes);

        auto popEmptyFreeArc = new PNTPArc(popEmpty, _freePlace, _depth);
        auto freePopEmptyArc = new PNPTArc(_freePlace, popEmpty, _depth);
        pni.pnes.insert(popEmptyFreeArc);
        pni.pnes.insert(freePopEmptyArc);
    }
    NonBlockingPipe(unsigned depth, string label) : BlockingPipe(depth,label)
    {
        _popPlace = new PNPlace(_label+".pop");
    }
};

class SignalPipe : public Pipe
{
using Pipe::Pipe;
protected:
    void pushOnFull() {}
    DatumBase* popOnEmpty() { return _store[lastPopPosOnEmpty()]; }
    void buildPNOport1(PNTransition *sreq, PNTransition *sack, PNInfo& pni) {}
    void buildPNIport1(PNTransition *ureq, PNTransition *uack, PNInfo& pni) {}
};

// There was a remark in Aa LRM that signal would block
// till first push happens. This behavior was later
// changed to returning 0. Implementation is retained for
// reference.
// class SignalPipe_Old : public Pipe
// {
//     PNPlace *_havePushPlace, *_awaitingPushPlace, *_donePushPlace;
//     PNTransition *_doFirstPush;
// protected:
//     void pushOnFull() {}
//     DatumBase* popOnEmpty() { return _store[lastPopPosOnEmpty()]; }
//     void initPN1() { _awaitingPushPlace->addtokens(_depth); }
//     void buildPNOport1(PNTransition *sreq, PNTransition *sack, Elements& pnes)
//     {
//         // out port sack should release a token to _havePushPlace
//         PetriNet::createArc(sreq, _havePushPlace, pnes);
//     }
//     void buildPNIport1(PNTransition *ureq, PNTransition *uack, Elements& pnes)
//     {
//         // in port should require token at _donePushPlace
//         PetriNet::createArc(_donePushPlace, sreq, pnes);
//         // in port should release a token to _donePushPlace
//         PetriNet::createArc(sack, _donePushPlace, pnes);
//     }
//     void buildPN(Elements& pnes)
//     {
//         PetriNet::createArc(_havePushPlace, _doFirstPush, pnes);
//         PetriNet::createArc(_awaitingPushPlace, _doFirstPush, pnes);
//         PetriNet::createArc(_doFirstPush, _donePushPlace, pnes);
//     }
//     SignalPipe_Old(unsigned depth, string label) : Pipe(depth,label)
//     {
//         _havePushPlace = new PNPlace(_label+".HavePush");
//         _awaitingPushPlace = new PNPlace("MARKP:"+_label+".AwaitingPush");
//         _donePushPlace = new PNPlace(_label+".DonePush");
//         _doFirstPush = new PNTransition(_label+".DoFirstPush");
//     }
// };

// Unfortunately constructor can't be templatized, so it has to be provided
template <typename T, typename Base> class Fifo : public Base
{
    ModCntr _rpos;
protected:
    unsigned poppos() { return _rpos++; }
    unsigned lastPopPosOnEmpty() { return _rpos.prev(); }
public:
    Fifo(unsigned depth, unsigned width, string label) : _rpos(ModCntr(depth)), Base(depth,label)
    {
        Pipe::initStore<T>(depth, width);
    }
};

// Lifo doesn't really need 2 markers for read and write, as such it doesn't need a circular buffer
template <typename T, typename Base> class Lifo : public Base
{
protected:
    unsigned poppos() { return --(this->_wpos); }
    unsigned lastPopPosOnEmpty() { return 0; }
public:
    Lifo(unsigned depth, unsigned width, string label) : Base(depth,label)
    {
        Pipe::initStore<T>(depth, width);
    }
};

// Pipe feeder and reader are for test bench use. Are not MT safe.
class PipeIf
{
protected:
    Pipe *_p;
    // Could do with single transition here, but just keeping it uniform with the rest
    PNTransition *_sreq, *_sack;
    PNPlace *_triggerPlace;
public:
    virtual void sack() = 0;
    virtual void _buildPN(PNInfo& pni) = 0;
    void buildPN(PNInfo& pni)
    {
        PetriNet::createArc(_triggerPlace, _sreq, pni.pnes);
        PetriNet::createArc(_sreq, _sack, pni.pnes);
        _buildPN(pni);
    }
    PipeIf(Pipe *p) : _p(p)
    {
        _sreq = new PNTransition("PipeIf_sreq");
        _sack = new PNTransition("PipeIf_sack");
        _sack->setEnabledActions(bind(&PipeIf::sack,this));
        _triggerPlace = new PNPlace("PipeIf_trigger");
    }
};

class PipeFeeder : public PipeIf
{
    queue<DatumBase*> _payloadq;
using PipeIf::PipeIf;
    void _buildPN(PNInfo& pni) { _p->buildPNOport(_sreq, _sack, pni); }
    void sack()
    {
        _p->push( _payloadq.front() );
        _payloadq.pop();
    }
public:
    void feed(vector<DatumBase*>& payload)
    {
        if ( not _payloadq.empty() )
        {
            cout << "pipes: queue not empty when feed is invoked" << endl;
            exit(1);
        }
        for(auto d:payload) _payloadq.push(d);
        _triggerPlace->addtokens(payload.size());
    }
};

class PipeReader : public PipeIf
{
using PipeIf::PipeIf;
    vector<DatumBase*> _retv;
    function<void()> _exitActions;
    unsigned _sz;
    void _buildPN(PNInfo& pni) { _p->buildPNIport(_sreq, _sack, pni); }
    void clear()
    {
        for(auto d:_retv) delete d;
        _retv.clear();
    }
public:
    void receive(unsigned sz, function<void()> exitActions)
    {
        clear();
        _sz = sz;
        _exitActions = exitActions;
        _triggerPlace->addtokens(sz);
    }
    vector<DatumBase*>& collect() { return _retv; }
    void sack()
    {
        DatumBase *popped = _p->pop();
        DatumBase *dptr = popped->clone();
        dptr->blindcopy(popped);
        _retv.push_back( dptr );
        if ( _retv.size() == _sz ) _exitActions();
        // There is no need to reset any attributes as next read cycle can
        // begin only after next receive call which would set fresh attributes
    }
};
#endif

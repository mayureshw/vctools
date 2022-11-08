#ifndef _PIPES_H
#define _PIPES_H

#include <iostream>
#include <vector>
#include <queue>
#include <condition_variable>
#include "datum.h"
#include "vcpetrinet.h"

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
    VcPetriNet *_pn;
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
    void buildPNMutexDep(PNTransition *req, PNTransition *ack)
    {
        // for i or o, req should seek token from mutexplace
        pn()->createArc(_mutexPlace, req);
        // for i or o, ack should release token to mutexplace
        pn()->createArc(ack, _mutexPlace);
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
    virtual void buildPNOport1(PNTransition *sreq, PNTransition *sack)=0;
    virtual void buildPNIport1(PNTransition *ureq, PNTransition *uack)=0;
public:
    VcPetriNet* pn() { return _pn; }
    string _label;
    bool empty() { return _nElems == 0; }
    bool full() { return _nElems == _depth; }
    void push(DatumBase* din, unsigned long eseqno)
    {
        if ( full() )
        {
            PIPELOG("pushfull:" << eseqno << ":" << _label << ":" << din->str())
            pushOnFull();
        }
        else
        {
            unsigned i = pushpos();
            _nElems++;
            _store[i]->blindcopy(din);
            PIPELOG("push:" << eseqno << ":" << _label << ":" << din->str())
        }
    }
    DatumBase* pop(unsigned long eseqno)
    {
        if ( empty() )
        {
            auto retval = popOnEmpty();
            PIPELOG("popempty:" << eseqno << ":" << _label << ":" << retval->str())
            return retval;
        }
        else
        {
            unsigned i = poppos();
            _nElems--;
            PIPELOG("pop:" << eseqno << ":" << _label << ":" << _store[i]->str())
            return _store[i];
        }
    }
    // Note that buildPN is for part of pipe's own network and buildPN[IO]port
    // are for its connections with pipe's users requests and acks
    // So buildPN is called once per pipe and buildPN[IO]port once per usage
    // instance of that pipe
    virtual void buildPN() {}
    void buildPNIport(PNTransition *ureq, PNTransition *uack)
    {
        buildPNMutexDep(ureq, uack);
        buildPNIport1(ureq, uack);
    }
    void buildPNOport(PNTransition *sreq, PNTransition *sack)
    {
        buildPNMutexDep(sreq, sack);
        buildPNOport1(sreq, sack);
    }
    Pipe(unsigned depth, string label, VcPetriNet* pn) : _depth(depth), _wpos(ModCntr(depth)), _label(label), _pn(pn)
    {
        _mutexPlace = _pn->createPlace("MARKP:"+_label+".Mutex",1);
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
    void buildPNOport1(PNTransition *sreq, PNTransition *sack)
    {
        // out port sreq should seek token from freePlace
        pn()->createArc(_freePlace, sreq);
        // out port sack should release a token to filledPlace
        pn()->createArc(sreq, _filledPlace);
    }
    void buildPNIport1(PNTransition *ureq, PNTransition *uack)
    {
        // in port ureq should need a token in filledPlace
        pn()->createArc(_filledPlace, ureq);
        // in port should release a token to freePlace on uack
        pn()->createArc(uack, _freePlace);
    }
    BlockingPipe(unsigned depth, string label, VcPetriNet *pn) : Pipe(depth,label,pn)
    {
        _freePlace = pn->createPlace("MARKP:"+_label+".Depth",_depth);
        _filledPlace = pn->createPlace(_label+".Filled");
    }
};

class NonBlockingPipe : public BlockingPipe
{
    PNPlace *_popPlace;
protected:
    DatumBase* popOnEmpty() { return _zero; }
    void buildPNIport1(PNTransition *ureq, PNTransition *uack)
    {
        pn()->createArc(ureq, _popPlace);
    }
public:
    void buildPN()
    {
        auto popEmpty = pn()->createTransition(_label+".popEmpty");
        auto popNonEmpty = pn()->createTransition(_label+".popNonEmpty");
        pn()->createArc(_popPlace, popEmpty);
        pn()->createArc(_popPlace, popNonEmpty);
        pn()->createArc(_filledPlace, popNonEmpty);
        pn()->createArc(popNonEmpty, _freePlace);

        auto popEmptyFreeArc = new PNTPArc(popEmpty, _freePlace, _depth);
        auto freePopEmptyArc = new PNPTArc(_freePlace, popEmpty, _depth);
    }
    NonBlockingPipe(unsigned depth, string label, VcPetriNet* pn) : BlockingPipe(depth,label,pn)
    {
        _popPlace = pn->createPlace(_label+".pop");
    }
};

class SignalPipe : public Pipe
{
using Pipe::Pipe;
protected:
    void pushOnFull() {}
    DatumBase* popOnEmpty() { return _store[lastPopPosOnEmpty()]; }
    void buildPNOport1(PNTransition *sreq, PNTransition *sack) {}
    void buildPNIport1(PNTransition *ureq, PNTransition *uack) {}
};

// Unfortunately constructor can't be templatized, so it has to be provided
template <typename T, typename Base> class Fifo : public Base
{
    ModCntr _rpos;
protected:
    unsigned poppos() { return _rpos++; }
    unsigned lastPopPosOnEmpty() { return _rpos.prev(); }
public:
    Fifo(unsigned depth, unsigned width, string label, VcPetriNet* pn) : _rpos(ModCntr(depth)), Base(depth,label,pn)
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
    Lifo(unsigned depth, unsigned width, string label, VcPetriNet* pn) : Base(depth,label,pn)
    {
        Pipe::initStore<T>(depth, width);
    }
};

class PipeIf
{
protected:
    Pipe *_p;
    // Could do with single transition here, but just keeping it uniform with the rest
    PNTransition *_sreq, *_sack;
    PNPlace *_triggerPlace;
    VcPetriNet* pn() { return _p->pn(); }
public:
    virtual void sack(unsigned long eseqno) = 0;
    virtual void _buildPN() = 0;
    void buildPN()
    {
        pn()->createArc(_triggerPlace, _sreq);
        pn()->createArc(_sreq, _sack);
        _buildPN();
    }
    PipeIf(Pipe *p) : _p(p)
    {
        _sreq = pn()->createTransition("PipeIf_sreq");
        _sack = pn()->createTransition("PipeIf_sack");
        _sack->setEnabledActions(bind(&PipeIf::sack,this,_1));
        _triggerPlace = pn()->createPlace("PipeIf_trigger");
    }
};

class PipeFeeder : public PipeIf
{
    mutex _qlock;
    queue<DatumBase*> _payloadq;
using PipeIf::PipeIf;
    void _buildPN() { _p->buildPNOport(_sreq, _sack); }
    void sack(unsigned long eseqno)
    {
        _qlock.lock();
        _p->push( _payloadq.front(), eseqno );
        _payloadq.pop();
        _qlock.unlock();
    }
public:
    void feed(const vector<DatumBase*>& payload)
    {
        _qlock.lock();
        for(auto d:payload) _payloadq.push(d);
        _qlock.unlock();
        pn()->addtokens(_triggerPlace, payload.size());
    }
};

class PipeReader : public PipeIf
{
using PipeIf::PipeIf;
    mutex _recd_mutex;
    condition_variable _recd_cvar;
    vector<DatumBase*> _retv;
    function<void()> _exitActions;
    bool _syncmode;
    unsigned _sz;
    void _buildPN() { _p->buildPNIport(_sreq, _sack); }
    void clear()
    {
        for(auto d:_retv) delete d;
        _retv.clear();
    }
    void receive_common(unsigned sz, bool syncmode, function<void()> exitActions)
    {
        _syncmode = syncmode;
        clear();
        _sz = sz;
        _exitActions = exitActions;
    }
public:
    // NOTE: Do not use both sync and async receive on the same pipe

    // A blocking call that waits to gather sz no of records, useful only if
    // there is a dedicated thread for reads.
    vector<DatumBase*>& receive_sync(unsigned sz)
    {
        receive_common(sz, true, [](){});
        unique_lock<mutex> ulockq(_recd_mutex);
        pn()->addtokens(_triggerPlace, sz);
        _recd_cvar.wait(ulockq);
        return _retv;
    }
    // A non blocking call that executes exitActions once sz no of records are
    // gathered, to be collected using collect useful for batch mode usage
    void receive_async(unsigned sz, function<void()> exitActions)
    {
        receive_common(sz, false, exitActions);
        pn()->addtokens(_triggerPlace, sz);
    }
    vector<DatumBase*>& collect() { return _retv; }
    void sack(unsigned long eseqno)
    {
        DatumBase *popped = _p->pop(eseqno);
        DatumBase *dptr = popped->clone();
        dptr->blindcopy(popped);
        _retv.push_back( dptr );
        if ( _retv.size() == _sz )
        {
            unique_lock<mutex> ulockq(_recd_mutex);
            if ( _syncmode ) _recd_cvar.notify_one();
            else _exitActions();
        }
        // There is no need to reset any attributes as next read cycle can
        // begin only after next receive call which would set fresh attributes
    }
};
#endif

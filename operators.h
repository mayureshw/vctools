#ifndef _OPERATORS_H
#define _OPERATORS_H

#include <vector>
#include <list>
#include <iostream>
#include "datum.h"
#include "pipes.h"

#ifdef OPDBG
    static ofstream oplog("ops.log");
    static mutex oplogmutex;
#   define OPLOG(ARGS) \
        oplogmutex.lock(); \
        oplog << ARGS << endl; \
        oplogmutex.unlock();
#else
#   define OPLOG(ARGS)
#endif

// It is advisable to use concrete functions as callbacks to save vtbl cost
// i.e. Plus<T>::sack is better than Operator:sack (but not achived currently)
// (Unfortunately uack still uses virtual function.)

// ----------- base classes for various operators --------------
class Operator
{
protected:
    vector<DatumBase*> _ipv;
    vector<DatumBase*> _resv;
    const string _dplabel; // for logging only
public:
    virtual string oplabel()=0;
    vector<DatumBase*> opv;
    // can't mandate some of these functions on all the operators, some are
    // even unique to a single operator, but have to keep them here to save
    // having to write template arguments where the function pointers are used.
    virtual void sack()
    {
        cout << "operators.h: sack not implemented " << endl;
        exit(1);
    }
    virtual void select(int i)
    {
        cout << "operators.h: select not implemented " << endl;
        exit(1);
    }
    // TODO: flowthrough can do minutely better by skipping _resv
    virtual void flowthrough() { sack(); uack(); }
    string inpvstr()
    {
        string retstr;
        for(auto inp:_ipv) retstr += inp->str() + ":";
        return retstr;
    }
    virtual void ureq()
    {
        cout << "operators.h: ureq not implemented " << endl;
        exit(1);
    }
    void uack()
    {
        for(int i=0; i<_resv.size(); i++)
        {
            OPLOG("uack" << to_string(i) << ":" << oplabel() << ":" << _dplabel << ":" << inpvstr() << _resv[i]->str())
            *opv[i] = _resv[i];
        }
    }
    void setinpv(const vector<DatumBase*>& ipv) { _ipv = ipv; }
    Operator(string label="") : _dplabel(label) {}
};

template <typename Tout, typename Tin1, typename Tin2, typename Tin3> class TernOperator
    : public Operator
{
protected:
    Datum<Tout> z, op;
    virtual Tout eval(Tin1& p, Tin2& q, Tin3& r, unsigned wp, unsigned wq, unsigned wr) = 0;
public:
    void sack()
    {
        auto p = (Tin1) *this->_ipv[0];
        auto q = (Tin2) *this->_ipv[1];
        auto r = (Tin3) *this->_ipv[2];
        unsigned wp = ( this->_ipv[0] )->width();
        unsigned wq = ( this->_ipv[1] )->width();
        unsigned wr = ( this->_ipv[1] )->width();
        z = eval(p,q,r,wp,wq,wr);
    }
    TernOperator(unsigned width, string label) : z(width), op(width), Operator(label)
    {
        _resv.push_back(&z);
        opv.push_back(&op);
    }
};

// Binary operators with 2 input args of types Tin1 and Tin2 and output of type Tout
template <typename Tout, typename Tin1, typename Tin2> class BinOperator : public Operator
{
protected:
    Datum<Tout> z, op;
    virtual Tout eval(Tin1& x, Tin2& y, unsigned wx, unsigned wy) = 0;
public:
    void sack()
    {
        auto x = (Tin1)*this->_ipv[0];
        auto y = (Tin2)*this->_ipv[1];
        unsigned wx = ( this->_ipv[0] )->width();
        unsigned wy = ( this->_ipv[1] )->width();
        z = eval(x,y,wx,wy);
    }
    BinOperator(unsigned width, string label) : z(width), op(width), Operator(label)
    {
        _resv.push_back(&z);
        opv.push_back(&op);
    }
};

#define BINOP(CLS,EXPR) template <typename Tout, typename Tin1, typename Tin2> class CLS : public BinOperator<Tout, Tin1, Tin2> \
{ \
using BinOperator<Tout, Tin1, Tin2>::BinOperator; \
public: \
    string oplabel() { return #CLS; } \
    Tout eval(Tin1& x, Tin2& y, unsigned wx, unsigned wy) { return EXPR ; } \
};

template <typename Tout, typename Tin> class UnaryOperator : public Operator
{
protected:
    Datum<Tout> z, op;
    virtual Tout eval(Tin& x, unsigned wx) = 0;
public:
    void sack()
    {
        auto x = (Tin) *this->_ipv[0];
        unsigned wx = ( this->_ipv[0] )->width();
        z = eval(x, wx);
    }
    UnaryOperator(unsigned width, string label) : z(width), op(width), Operator(label)
    {
        _resv.push_back(&z);
        opv.push_back(&op);
    }
};

class IOPort : public Operator
{
public:
    Pipe* _pipe;
    // width passed due to macro uniformity, ignored
    IOPort(unsigned width, string label, Pipe *pipe) : _pipe(pipe), Operator(label) {}
};

// -------- concrete operator classes below this -------------

BINOP( Plus,   x + y  ) 
BINOP( Minus,  x - y  )
BINOP( Mult,   x * y  )
BINOP( And,    x & y  )
BINOP( Or,     x | y  )
BINOP( Lt,     x < y  )
BINOP( Gt,     x > y  )
BINOP( Ge,     x >= y )
BINOP( Ne,     x != y )
BINOP( Eq,     x == y )
BINOP( Concat, ( x << wy ) | y )
BINOP( Bitsel, ( ( x >> y ) & 1 ) ? 1 : 0 )

template <typename Tout, typename Tin> class Not : public UnaryOperator<Tout, Tin>
{
using UnaryOperator<Tout, Tin>::UnaryOperator;
public:
    string oplabel() { return "Not"; }
    Tout eval(Tin& x, unsigned wx) { return ~x&(1<<(wx+1)-1); }
};

template <typename Tout, typename Tin1, typename Tin2, typename Tin3> class Select : public TernOperator<Tout, Tin1, Tin2, Tin3>
{
using TernOperator<Tout, Tin1, Tin2, Tin3>::TernOperator;
public:
    string oplabel() { return "Select"; }
    Tout eval(Tin1& p, Tin2& q, Tin3& r, unsigned pw, unsigned qw, unsigned rw) { return p ? q : r; }
};

template <typename Tout, typename Tin> class Slice : public Operator
{
    Datum<Tout> y, op;
    const unsigned long _mask;
    const unsigned _l;
    unsigned long getmask(unsigned h, unsigned l)
    {
        const unsigned long one = 1;
        // can't use 1<<(h+1) as if h is msb position, results are undefined
        unsigned long hmask = ( ( ( one << h ) - 1 ) << 1 ) + 1;
        unsigned long lmask = ( ( one << l ) - 1 );
        return hmask & ~lmask;
    }
public:
    string oplabel() { return "Slice"; }
    void sack()
    {
        auto x = (Tout) *this->_ipv[0];
        y = ( _mask & x ) >> _l;
    }
    Slice(unsigned width, string label, unsigned h, unsigned l) : y(width), op(width), _l(l),
        _mask(getmask(h,l)), Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename Tout, typename Tin> class Assign : public Operator
{
    Datum<Tout> y, op;
public:
    string oplabel() { return "Assign"; }
    void sack()
    {
        auto x = (Tout) *this->_ipv[0];
        y = x;
    }
    Assign(unsigned width, string label): y(width), op(width), Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename Tin> class Branch : public Operator
{
    Datum<Tin> y = {1}, op = {1};
    const list<int>
        chooseFalse = {0},
        chooseTrue = {1};
public:
    string oplabel() { return "Branch"; }
    list<int> arcChooser()
    {
        auto x = (Tin) *this->_ipv[0];
        return x ? chooseTrue : chooseFalse;
    }
    Branch(unsigned width, string label) : Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

#define BRANCH Branch<uint8_t>

template <typename Tout, typename Tin1, typename Tin2> class Phi : public Operator
{
    Datum<Tout> y, op;
public:
    string oplabel() { return "Phi"; }
    void select(int i)
    {
        auto x = (Tin1) *this->_ipv[i];
        y = x;
    }
    Phi(unsigned width, string label) : y(width), op(width), Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename Tout, typename Tin> class Load : public Operator
{
    vector<DatumBase*>& _stv;
    Datum<Tout> y, op;
public:
    string oplabel() { return "Load"; }
    void sack()
    {
        auto i = (Tin) *this->_ipv[0];
        y = (Tout) *_stv[i];
    }
    Load(unsigned width, string label, vector<DatumBase*>& stv) : _stv(stv), y(width), op(width), Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename Tin1, typename Tin2> class Store : public Operator
{
    vector<DatumBase*>& _stv;
public:
    string oplabel() { return "Store"; }
    void sack()
    {
        auto i = (Tin1) *this->_ipv[0];
        auto val = (Tin2) *this->_ipv[1];
        *((Datum<Tin2>*)_stv[i]) = val;
        // Since Store doesn't have a uack log, we log its sack
        OPLOG("sack:" << oplabel() << ":" << _dplabel << ":" << i << ":" << val)
    }
    // width passed due to macro uniformity, ignored
    Store(unsigned width, string label, vector<DatumBase*>& stv) : _stv(stv), Operator(label) {}
};

template <typename Tout> class Inport : public IOPort
{
    Datum<Tout> y, op;
public:
    string oplabel() { return "Inport"; }
    // AHIR does read operations on update events, unlike other operators
    // Hence we implement uack like operations on ureq and customize flowthrough
    void flowthrough() { ureq(); uack(); }
    void ureq()
    {
        y = (Tout) *_pipe->pop();
    }
    Inport(unsigned width, string label, Pipe *pipe) : IOPort(width,label,pipe), y(width), op(width)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename Tin> class Outport : public IOPort
{
using IOPort::IOPort;
public:
    string oplabel() { return "Outport"; }
    void sack()
    {
        _pipe->push(_ipv[0]);
    }
};

class Call : public Operator
{
    vector<DatumBase*> _moduleipv;
public:
    string oplabel() { return "Call"; }
    void sack()
    {
        for(int i=0; i<_ipv.size(); i++) *_moduleipv[i] = _ipv[i];
    }
    Call(vector<DatumBase*>& moduleipv, vector<DatumBase*>& moduleopv, string label) : _moduleipv(moduleipv), Operator(label)
    {
        _resv = moduleopv;
        for(auto d:_resv) opv.push_back(d->clone());
    }
    ~Call() { for(auto d:opv) delete d; }
};

// A placeholder operator for under development ones
template <typename T> class Generic : public Operator
{
    Datum<T> op;
public:
    string oplabel() { return "Generic"; }
    void sack()
    {
        cout << "WARNING: Generic::sack called. This is a stub. Inputs are" << endl;
        for(auto d:_ipv) cout << "\t" << d->str() << endl;
    }
    Generic(unsigned width, string label) : op(width), Operator(label)
    {
        cout << "WARNING: operator Generic called. This is a stub" << endl;
        opv.push_back(&op);
    }
};

#endif

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

// For type compatibility reason, BOOL is defined as unsgined. Use of BOOL will
// make the intention clearer than directly using unsigned
#define BOOL unsigned

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

template <typename Tin1, typename Tin2, typename Tin3, typename Tout> class TernOperatorBase
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
    TernOperatorBase(unsigned width, string label) : z(width), op(width), Operator(label)
    {
        _resv.push_back(&z);
        opv.push_back(&op);
    }
};

// Binary operators with 2 input args of types Tin1 and Tin2 and output of type Tout
template <typename Tin1, typename Tin2, typename Tout> class BinOperatorBase : public Operator
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
    BinOperatorBase(unsigned width, string label) : z(width), op(width), Operator(label)
    {
        _resv.push_back(&z);
        opv.push_back(&op);
    }
};

template <typename T> class UnaryOperator : public Operator
{
protected:
    Datum<T> z, op;
    virtual T eval(T& x, unsigned wx) = 0;
public:
    void sack()
    {
        auto x = (T) *this->_ipv[0];
        unsigned wx = ( this->_ipv[0] )->width();
        z = eval(x, wx);
    }
    UnaryOperator(unsigned width, string label) : z(width), op(width), Operator(label)
    {
        _resv.push_back(&z);
        opv.push_back(&op);
    }
};

// Binary operators with 2 input args of type Tin and 1 output of type Tout
template <typename Tin, typename Tout> class BinOperator : public BinOperatorBase<Tin, Tin, Tout>
{using BinOperatorBase<Tin, Tin, Tout>::BinOperatorBase;};

// Arithmetic and Logic operators where input and output type is same i.e. T
template <typename T> class BinOperatorAL : public BinOperator<T,T>
{using BinOperator<T,T>::BinOperator;};

// Relational operators where output type is BOOL, input is T
template <typename T> class BinOperatorR : public BinOperator<T,BOOL>
{using BinOperator<T,BOOL>::BinOperator;};

// -------- concrete operator classes below this -------------

template <typename T> class Plus : public BinOperatorAL<T>
{
using BinOperatorAL<T>::BinOperatorAL;
public:
    string oplabel() { return "Plus"; }
    T eval(T& x, T& y, unsigned, unsigned) { return x + y; }
};

template <typename T> class Minus : public BinOperatorAL<T>
{
using BinOperatorAL<T>::BinOperatorAL;
public:
    string oplabel() { return "Minus"; }
    T eval(T& x, T& y, unsigned, unsigned) { return x - y; }
};

template <typename T> class Mult : public BinOperatorAL<T>
{
using BinOperatorAL<T>::BinOperatorAL;
public:
    string oplabel() { return "Mult"; }
    T eval(T& x, T& y, unsigned, unsigned) { return x * y; }
};

template <typename Tin1, typename Tin2, typename Tout> class Concat : public BinOperatorBase<Tin1,Tin2,Tout>
{
using BinOperatorBase<Tin1,Tin2,Tout>::BinOperatorBase;
public:
    string oplabel() { return "Concat"; }
    Tout eval(Tin1& x, Tin2& y, unsigned, unsigned wy) { return (x<<wy)|y; }
};

template <typename T> class And : public BinOperatorAL<T>
{
using BinOperatorAL<T>::BinOperatorAL;
public:
    string oplabel() { return "And"; }
    T eval(T& x, T& y, unsigned, unsigned) { return x&y; }
};

template <typename T> class Or : public BinOperatorAL<T>
{
using BinOperatorAL<T>::BinOperatorAL;
public:
    string oplabel() { return "Or"; }
    T eval(T& x, T& y, unsigned, unsigned) { return x|y; }
};

template <typename T> class Not : public UnaryOperator<T>
{
using UnaryOperator<T>::UnaryOperator;
public:
    string oplabel() { return "Not"; }
    T eval(T& x, unsigned wx) { return ~x&(1<<(wx+1)-1); }
};

template <typename T> class Lt : public BinOperatorR<T>
{
using BinOperatorR<T>::BinOperatorR;
public:
    string oplabel() { return "Lt"; }
    BOOL eval(T& x, T& y, unsigned, unsigned) { return x < y; }
};

template <typename T> class Gt : public BinOperatorR<T>
{
using BinOperatorR<T>::BinOperatorR;
public:
    string oplabel() { return "Gt"; }
    BOOL eval(T& x, T& y, unsigned, unsigned) { return x > y; }
};

template <typename T> class Ge : public BinOperatorR<T>
{
using BinOperatorR<T>::BinOperatorR;
public:
    string oplabel() { return "Ge"; }
    BOOL eval(T& x, T& y, unsigned, unsigned) { return x >= y; }
};

template <typename T> class Ne : public BinOperatorR<T>
{
using BinOperatorR<T>::BinOperatorR;
public:
    string oplabel() { return "Ne"; }
    BOOL eval(T& x, T& y, unsigned, unsigned) { return x != y; }
};

template <typename T> class Eq : public BinOperatorR<T>
{
using BinOperatorR<T>::BinOperatorR;
public:
    string oplabel() { return "Eq"; }
    BOOL eval(T& x, T& y, unsigned, unsigned) { return x == y; }
};

template <typename T> class Bitsel : public BinOperatorBase<T,int,BOOL>
{
using BinOperatorBase<T, int, BOOL>::BinOperatorBase;
public:
    string oplabel() { return "Bitsel"; }
    BOOL eval(T& x, int& y, unsigned, unsigned) { return ( ( x >> y ) & 1 ) ? 1 : 0; }
};

template <typename T> class Select : public TernOperatorBase<BOOL,T,T,T>
{
using TernOperatorBase<BOOL,T,T,T>::TernOperatorBase;
public:
    string oplabel() { return "Select"; }
    T eval(BOOL& p, T& q, T& r, unsigned pw, unsigned qw, unsigned rw) { return p ? q : r; }
};

template <typename T> class Slice : public Operator
{
    Datum<T> y, op;
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
        auto x = (T) *this->_ipv[0];
        y = ( _mask & x ) >> _l;
    }
    Slice(unsigned width, string label, unsigned h, unsigned l) : y(width), op(width), _l(l),
        _mask(getmask(h,l)), Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename T> class Assign : public Operator
{
    Datum<T> y, op;
public:
    string oplabel() { return "Assign"; }
    void sack()
    {
        auto x = (T) *this->_ipv[0];
        y = x;
    }
    Assign(unsigned width, string label): y(width), op(width), Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

class Branch : public Operator
{
    Datum<BOOL> y = {1}, op = {1};
    const list<int>
        chooseFalse = {0},
        chooseTrue = {1};
public:
    string oplabel() { return "Branch"; }
    list<int> arcChooser()
    {
        auto x = (BOOL) *this->_ipv[0];
        return x ? chooseTrue : chooseFalse;
    }
    Branch(string label) : Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename T> class Phi : public Operator
{
    Datum<T> y, op;
public:
    string oplabel() { return "Phi"; }
    void select(int i)
    {
        auto x = (T) *this->_ipv[i];
        y = x;
    }
    Phi(unsigned width, string label) : y(width), op(width), Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename T> class Load : public Operator
{
    vector<DatumBase*>& _stv;
    Datum<T> y, op;
public:
    string oplabel() { return "Load"; }
    void sack()
    {
        auto i = (unsigned) *this->_ipv[0];
        y = (T) *_stv[i];
    }
    Load(unsigned width, string label, vector<DatumBase*>& stv) : _stv(stv), y(width), op(width), Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

template <typename T> class Store : public Operator
{
    vector<DatumBase*>& _stv;
public:
    string oplabel() { return "Store"; }
    void sack()
    {
        auto i = (unsigned) *this->_ipv[0];
        auto val = (T) *this->_ipv[1];
        *((Datum<T>*)_stv[i]) = val;
        // Since Store doesn't have a uack log, we log its sack
        OPLOG("sack:" << oplabel() << ":" << _dplabel << ":" << i << ":" << val)
    }
    // width passed due to macro uniformity, ignored
    Store(unsigned width, string label, vector<DatumBase*>& stv) : _stv(stv), Operator(label) {}
};

class IOPort : public Operator
{
public:
    Pipe* _pipe;
    // width passed due to macro uniformity, ignored
    IOPort(unsigned width, string label, Pipe *pipe) : _pipe(pipe), Operator(label) {}
};

// Outport doesn't need template parameter, kept for macro uniformity
template <typename T> class Outport : public IOPort
{
using IOPort::IOPort;
public:
    string oplabel() { return "Outport"; }
    void sack()
    {
        _pipe->push(_ipv[0]);
    }
};

template <typename T> class Inport : public IOPort
{
    Datum<T> y, op;
public:
    string oplabel() { return "Inport"; }
    // AHIR does read operations on update events, unlike other operators
    // Hence we implement uack like operations on ureq and customize flowthrough
    void flowthrough() { ureq(); uack(); }
    void ureq()
    {
        y = (T) *_pipe->pop();
    }
    Inport(unsigned width, string label, Pipe *pipe) : IOPort(width,label,pipe), y(width), op(width)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
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

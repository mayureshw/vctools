#ifndef _OPERATORS_H
#define _OPERATORS_H

#include <vector>
#include <list>
#include <iostream>
#include "datum.h"
#include "pipes.h"
#include "opf.h"

#ifdef OPDBG
#   define OPLOG(ARGS) \
        oplogmutex.lock(); \
        oplog << ARGS << endl; \
        oplogmutex.unlock();
#else
#   define OPLOG(ARGS)
#endif

#define INPVAL(T,I) ((Datum<T>*)this->_ipv[I])->val
#define INPWIDTH(I) this->_ipv[I]->width()

// ----------- base classes for various operators --------------
class Operator
{
protected:
    vector<DatumBase*> _ipv;
    vector<DatumBase*> _resv;
    template<typename T> T getmask(unsigned width)
    {
        unsigned lead0s;
        if constexpr ( ISWUINT(T) ) lead0s = ( WIDEUINTSZ - width );
        else lead0s =  ( sizeof(T)*8 - width );
        return ( (T) ~((T)0) ) >> lead0s;
    }
public:
    const string _dplabel; // for logging only
#ifdef OPDBG
    inline static ofstream oplog;
    inline static mutex oplogmutex;
#endif
    static void setLogfile(string logfile)
    {
#ifdef OPDBG
        oplog.open(logfile);
#endif
    }
    virtual string oplabel()=0;
    vector<DatumBase*> opv;
    // can't mandate some of these functions on all the operators, some are
    // even unique to a single operator, but have to keep them here to save
    // having to write template arguments where the function pointers are used.
    virtual void sack(unsigned long eseqno)
    {
        cout << "operators.h: sack not implemented " << endl;
        exit(1);
    }
    virtual void select(int i, unsigned long eseqno)
    {
        cout << "operators.h: select not implemented " << endl;
        exit(1);
    }
    // TODO: flowthrough can do minutely better by skipping _resv
    virtual void flowthrough(unsigned long eseqno) { sack(eseqno); uack(eseqno); }
    string inpvstr()
    {
        string retstr;
        for(auto inp:_ipv) retstr += inp->str() + ":";
        return retstr;
    }
    virtual void ureq(unsigned long eseqno)
    {
        cout << "operators.h: ureq not implemented " << endl;
        exit(1);
    }
    void uack(unsigned long eseqno)
    {
        for(int i=0; i<_resv.size(); i++)
        {
            OPLOG("uack" << i << ":" << eseqno << ":" << oplabel() << ":" << _dplabel << ":" << inpvstr() << _resv[i]->str())
            opv[i]->blindcopy(_resv[i]);
        }
    }
    void setinpv(const vector<DatumBase*>& ipv) { _ipv = ipv; }
    Operator(string label) : _dplabel(label) {}
};

template <typename Tout> class OperatorT : public Operator
{
protected:
    Tout _mask = 0;
    Datum<Tout> z, op;
    Tout mask(Tout val)
    {
        if constexpr ( is_floating_point<Tout>::value ) return val;
        else return val & this->_mask;
    }
public:
    OperatorT(unsigned width, string label="") : z(width), op(width), Operator(label)
    {
        _resv.push_back(&z);
        opv.push_back(&op);
        if constexpr ( ! is_floating_point<Tout>::value )
            _mask = getmask<Tout>(width);
    }
};

template <typename Tout, typename Tin1, typename Tin2, typename Tin3> class TernOperator
    : public OperatorT<Tout>
{
using OperatorT<Tout>::OperatorT;
protected:
    virtual Tout eval(Tin1 p, Tin2 q, Tin3 r) = 0;
public:
    void sack(unsigned long eseqno)
    {
        auto p = INPVAL(Tin1, 0);
        auto q = INPVAL(Tin2, 1);
        auto r = INPVAL(Tin3, 2);
        this->z = this->mask( eval(p,q,r) );
    }
};

// Binary operators with 2 input args of types Tin1 and Tin2 and output of type Tout
template <typename Tout, typename Tin1, typename Tin2> class BinOperator : public OperatorT<Tout>
{
using OperatorT<Tout>::OperatorT;
protected:
    // By convention msbmask is taken for 0th argument of inpv
    Tin1 msbmask() { return 1 << ( this->_ipv[0]->width() - 1 ); }
    bool isNegative(Tout val) { return ( msbmask() & val ) != 0; }
    bool signedLT(Tout val1, Tout val2)
    {
        bool val1Neg = isNegative(val1);
        bool val2Neg = isNegative(val2);
        // if both have same sign bit, let unsigned comparison decide
        if ( val1Neg == val2Neg ) return val1 < val2;
        // Hereafter both values don't have same sign bit
        if ( val1Neg ) return true; // i.e. val1 is negative and val2 is non negative
        return false; // val2 is negative and val1 is non negative
    }
    virtual Tout eval(Tin1 x, Tin2 y) = 0;
public:
    void sack(unsigned long eseqno)
    {
        auto x = INPVAL(Tin1,0);
        auto y = INPVAL(Tin2,1);
        this->z = this->mask( eval(x,y) );
    }
};

#define BINOP(CLS,EXPR) template <typename Tout, typename Tin1, typename Tin2> class CLS : public BinOperator<Tout, Tin1, Tin2> \
{ \
using BinOperator<Tout, Tin1, Tin2>::BinOperator; \
public: \
    string oplabel() { return #CLS; } \
    Tout eval(Tin1 x, Tin2 y) { return EXPR ; } \
};

// For operators that allow 2nd arg to be WUINT_, but use it as long, expression should be in terms of x and yint
#define BINOP2(CLS,EXPR) template <typename Tout, typename Tin1, typename Tin2> class CLS : public BinOperator<Tout, Tin1, Tin2> \
{ \
using BinOperator<Tout, Tin1, Tin2>::BinOperator; \
public: \
    string oplabel() { return #CLS; } \
    Tout eval(Tin1 x, Tin2 y) \
    { \
        if constexpr ( ISWUINT(Tin2) ) \
        { \
            auto yint = y.to_ulong(); \
            return EXPR; \
        } \
        else \
        { \
            auto yint = y; \
            return EXPR; \
        } \
    } \
};

template <typename Tout, typename Tin> class UnaryOperator : public OperatorT<Tout>
{
using OperatorT<Tout>::OperatorT;
protected:
    virtual Tout eval(Tin x) = 0;
public:
    void sack(unsigned long eseqno)
    {
        auto x = INPVAL(Tin,0);
        this->z = this->mask( eval(x) );
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
BINOP( Div,    x / y  )
BINOP( And,    x & y  )
BINOP( Or,     x | y  )
BINOP( Xor,    x ^ y  )
BINOP( Nand,   ~ ( x & y ) )
BINOP( Nor,    ~ ( x | y ) )
BINOP( Xnor,   ~ ( x ^ y ) )
BINOP( Lt,     x < y  )
BINOP( Le,     x <= y )
BINOP( Gt,     x > y  )
BINOP( Ge,     x >= y )
BINOP( Ne,     x != y )
BINOP( Eq,     x == y )
BINOP( Concat, ( ((Tout) x) << INPWIDTH(1) ) | (Tout) y )

BINOP( SLt, this->signedLT(x,y)             )
BINOP( SLe, this->signedLT(x,y) || (x == y) )
BINOP( SGt, this->signedLT(y,x)             )
BINOP( SGe, this->signedLT(y,x) || (x == y) )

BINOP2( Bitsel, ( ( x >> yint ) & (Tin1) 1 ) != 0 )
BINOP2( ShiftL, x << yint )
BINOP2( ShiftR, x >> yint )

template <typename Tout, typename Tin1, typename Tin2> class Rotate : public BinOperator<Tout, Tin1, Tin2>
{
using BinOperator<Tout, Tin1, Tin2>::BinOperator;
protected:
    unsigned rotateby(Tin2 y)
    {
        unsigned yint;
        if constexpr ( ISWUINT(Tin2) ) yint = y.to_ulong();
        else yint = y;
        return yint % this->op.width();
    }
    Tout swap(Tin1 x, unsigned pos)
    {
        auto lmask = this->template getmask<Tout>(pos);
        auto hmask = this->_mask ^ lmask;
        return ( ( x & lmask ) << ( this->op.width() - pos ) ) |
            ( ( x & hmask ) >> pos );
    }
};

template <typename Tout, typename Tin1, typename Tin2> class RotateL : public Rotate<Tout, Tin1, Tin2>
{
using Rotate<Tout, Tin1, Tin2>::Rotate;
public:
    string oplabel() { return "RotateL"; }
    Tout eval(Tin1 x, Tin2 y) { return this->swap(x, this->op.width() - this->rotateby(y)); }
};

template <typename Tout, typename Tin1, typename Tin2> class RotateR : public Rotate<Tout, Tin1, Tin2>
{
using Rotate<Tout, Tin1, Tin2>::Rotate;
public:
    string oplabel() { return "RotateR"; }
    Tout eval(Tin1 x, Tin2 y) { return this->swap(x, this->rotateby(y)); }
};

template <typename Tout, typename Tin1, typename Tin2> class ShiftRA : public BinOperator<Tout, Tin1, Tin2>
{
using BinOperator<Tout, Tin1, Tin2>::BinOperator;
public:
    string oplabel() { return "ShiftRA"; }
    Tout eval(Tin1 x, Tin2 y)
    {
        if ( not this->isNegative(x) ) return x >> y;
        Tout retval = x;
        for(int i=0; i<y; i++) retval = ( retval >> 1 ) | this->msbmask();
        return retval;
    }
};

template <typename Tout, typename Tin> class Not : public UnaryOperator<Tout, Tin>
{
using UnaryOperator<Tout, Tin>::UnaryOperator;
public:
    string oplabel() { return "Not"; }
    Tout eval(Tin x) { return this->mask( ~x ); }
};

template <typename Tout, typename Tin> class CastOperator : public UnaryOperator<Tout, Tin>
{
protected:
    Tin _imsbmask, _inomsbmask;
    Tout _omsbmask, _onomsbmask, _replcarrymask;
public:
    CastOperator(unsigned owidth, unsigned iwidth, string label="") : UnaryOperator<Tout,Tin>(owidth, label)
    {
        _imsbmask = 1 << (iwidth - 1);
        _omsbmask = 1 << (owidth - 1);
        _inomsbmask = this->template getmask<Tin>(iwidth - 1);
        _onomsbmask = this->template getmask<Tout>(owidth - 1);
        _replcarrymask = ( owidth < iwidth ) ? _omsbmask : this->_mask ^ _inomsbmask;
    }
};

template <typename Tout, typename Tin> class S2S : public CastOperator<Tout, Tin>
{
using CastOperator<Tout, Tin>::CastOperator;
public:
    string oplabel() { return "S2S"; }
    Tout eval(Tin x)
    {
        return ( (this->_imsbmask & x) == 0 ) ? x : this->_replcarrymask | x;
    }
};

template <typename Tout, typename Tin1, typename Tin2, typename Tin3> class Select : public TernOperator<Tout, Tin1, Tin2, Tin3>
{
using TernOperator<Tout, Tin1, Tin2, Tin3>::TernOperator;
public:
    string oplabel() { return "Select"; }
    Tout eval(Tin1 p, Tin2 q, Tin3 r) { return p ? q : r; }
};

template <typename Tout, typename Tin> class Slice : public OperatorT<Tout>
{
    const unsigned _l;
public:
    string oplabel() { return "Slice"; }
    void sack(unsigned long eseqno)
    {
        auto x = INPVAL(Tin,0);
        if constexpr ( ISWUINT(Tin) and !ISWUINT(Tout) )
        {
            bitset<sizeof(unsigned long)> z1 = 0;
            for(int i=0, b = _l; i < this->z.width(); i++, b++) z1[i] = x[b];
            this->z = z1.to_ulong();
        }
        else this->z = this->mask( ( x >> _l ) );
    }
    Slice(unsigned width, string label, unsigned l) : _l(l),
        OperatorT<Tout>(width, label) {}
};

template <typename Tout, typename Tin> class Assign : public OperatorT<Tout>
{
using OperatorT<Tout>::OperatorT;
public:
    string oplabel() { return "Assign"; }
    void sack(unsigned long eseqno)
    {
        auto x = INPVAL(Tin,0);
        if constexpr ( ISWUINT(Tin) and !ISWUINT(Tout) )
            this->z = this->mask( x.to_ulong() );
        else
            this->z = this->mask( (Tout) x );
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
        auto x = INPVAL(Tin,0);
        return x ? chooseTrue : chooseFalse;
    }
    Branch(unsigned width, string label) : Operator(label)
    {
        _resv.push_back(&y);
        opv.push_back(&op);
    }
};

#define BRANCH Branch<uint8_t>

template <typename Tout, typename Tin1, typename Tin2> class Phi : public OperatorT<Tout>
{
using OperatorT<Tout>::OperatorT;
public:
    string oplabel() { return "Phi"; }
    void select(int i, unsigned long eseqno) { this->z = i == 0 ? INPVAL(Tin1,0) : INPVAL(Tin2,1); }
};

template <typename Tout, typename Tin> class Load : public OperatorT<Tout>
{
    vector<DatumBase*>& _stv;
public:
    string oplabel() { return "Load"; }
    void sack(unsigned long eseqno)
    {
        auto i = INPVAL(Tin,0);
        this->z = ((Datum<Tout>*) _stv[i])->val;
    }
    Load(unsigned width, string label, vector<DatumBase*>& stv) : _stv(stv), OperatorT<Tout>(width, label)
    {}
};

template <typename Tin1, typename Tin2> class Store : public Operator
{
    vector<DatumBase*>& _stv;
public:
    string oplabel() { return "Store"; }
    void sack(unsigned long eseqno)
    {
        auto i = INPVAL(Tin1,0);
        auto val = INPVAL(Tin2,1);
        ((Datum<Tin2>*)_stv[i])->val = val;
        // Since Store doesn't have a uack log, we log its sack
        OPLOG("sack:" << eseqno << ":" << oplabel() << ":" << _dplabel << ":" << to_string(i) << ":" << _ipv[1]->str())
    }
    // width passed due to macro uniformity, ignored
    Store(unsigned width, string label, vector<DatumBase*>& stv) : _stv(stv), Operator(label) {}
};

template <typename Tout> class Inport : public IOPort
{
    Datum<Tout> z, op;
public:
    string oplabel() { return "Inport"; }
    // AHIR does read operations on update events, unlike other operators
    // Hence we implement uack like operations on ureq and customize flowthrough
    void flowthrough(unsigned long eseqno) { ureq(eseqno); uack(eseqno); }
    void ureq(unsigned long eseqno)
    {
        z = ((Datum<Tout>*) _pipe->pop(eseqno))->val;
    }
    Inport(unsigned width, string label, Pipe *pipe) : IOPort(width,label,pipe), z(width), op(width)
    {
        _resv.push_back(&z);
        opv.push_back(&op);
    }
};

template <typename Tin> class Outport : public IOPort
{
using IOPort::IOPort;
public:
    string oplabel() { return "Outport"; }
    void sack(unsigned long eseqno) { _pipe->push(_ipv[0], eseqno); }
};

class Call : public Operator
{
    vector<DatumBase*> _moduleipv;
public:
    string oplabel() { return "Call"; }
    void sack(unsigned long eseqno)
    {
        for(int i=0; i<_ipv.size(); i++) _moduleipv[i]->blindcopy(_ipv[i]);
    }
    Call(vector<DatumBase*>& moduleipv, vector<DatumBase*>& moduleopv, string label) :
        _moduleipv(moduleipv), Operator(label)
    {
        _resv = moduleopv;
        for(auto d:_resv) opv.push_back(d->clone());
    }
    ~Call() { for(auto d:opv) delete d; }
};

#endif

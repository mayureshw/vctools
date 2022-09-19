#ifndef _DATUM_H
#define _DATUM_H

#include <iostream>
#include <string>
#include <bitset>
#include "opf.h"

class DatumBase
{
protected:
    const unsigned _width;
public:
    virtual DatumBase* clone() = 0;
    virtual string str() = 0;
    virtual void operator = (string&) = 0;
    // Deliberately named 'blind'. Caller takes responsibility of type safety
    // both Datums involved should be of same type. For performance reason we
    // do not validate types and refrain from using dynamic_casst
    // Do test with DATUMDBG whenever a new call to blindcopy is added
    virtual void blindcopy(DatumBase*) = 0;
    unsigned width() { return _width; }
    DatumBase(unsigned width) : _width(width) {}
    virtual ~DatumBase() {}
};

template <typename T> class Datum : public DatumBase
{
using DatumBase::DatumBase;
public:
    T val;
    // See comments in DatumBase
    void blindcopy(DatumBase *other)
    {
#       ifdef DATUMDBG
        assert( dynamic_cast<Datum<T>*>(other) );
#       endif
        val = ( (Datum<T>*) other )->val;
    }
    void operator = (T val_) { val = val_; }
    void operator = (string& bitstring)
    {
        if constexpr ( ISWUINT(T) )
        {
            if ( bitstring.size() > WIDEUINTSZ )
            {
                cout << "bitstring overflow for datum " << bitstring;
                cout << " Wide int size (configurable) limit is: " << WIDEUINTSZ << " sought " << bitstring.size() << endl;
            }
            string revbitstring = string(bitstring.rbegin(), bitstring.rend());
            val = WUINT(revbitstring);
        }
        else
        {
            // We don't use stoi, or hacks like *(int *)&val = etc. as the method
            // needs to work for various sizes of val
            auto valbits = sizeof(val) << 3;
            if ( bitstring.size() > valbits )
            {
                cout << "bitstring overflow for datum " << bitstring << " size=" << valbits << endl;
                exit(1);
            }
            val = 0;
            // NOTE: WE ARE ENDIAN SENSITIVE HERE - LITTLE ENDIAN IS SUPPORTED
            char* byteptr = (char*) &val;
            char mask = 1;
            for(int i=0; i < bitstring.size(); i++)
            {
                if( bitstring[i] == '1' ) *byteptr |= mask;
                mask <<=1;
                if ( mask == 0 )
                {
                    byteptr++;
                    mask = 1;
                }
            }
        }
    }
    DatumBase* clone() { return new Datum<T>(_width); } // caller to manage delete
    string str()
    {
        if constexpr ( ISWUINT(T) ) return val.to_string();
        else return to_string(val);
    }
};

#endif

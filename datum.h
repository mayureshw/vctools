#ifndef _DATUM_H
#define _DATUM_H

#include <iostream>
#include <string>

class DatumBase
{
public:
    virtual void operator = (DatumBase*) = 0;
    virtual void operator = (string& bitstring) = 0;
    virtual DatumBase* clone() = 0;
    virtual string str() = 0;
    virtual ~DatumBase() {}
    virtual unsigned width() = 0;
    virtual operator unsigned long () = 0;
};

template <typename T> class Datum : public DatumBase
{
    const unsigned _width;
public:
    T val;
    unsigned width() { return _width; }
    void operator = (DatumBase* other) { val = (T)*other; }
    void operator = (T val_) { val = val_; }
    // If T were to be string, there will be ambiguity, currently T can't be string
    void operator = (string& bitstring)
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
    DatumBase* clone() { return new Datum<T>(_width); } // caller to manage delete
    string str() { return to_string(val); }
    operator unsigned long () { return (unsigned long) val; }
    Datum(unsigned width) : _width(width)
    {
        auto tsz = sizeof(T)<<3;
        if ( width > tsz )
        {
            cout << "Attempt to instantiate datum of width=" << width << " with type size=" << tsz << endl;
            exit(1);
        }
    }
};

#endif

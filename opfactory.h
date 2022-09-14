#ifndef _OPFACTORY_H
#define _OPFACTORY_H

#include "operators.h"
#include "opf.h"


class OpFactory
{
    typedef enum Ctyp CTYPENUM;
    typedef pair< string, vector<Ctyp> > Opfkey;
    typedef function< Operator* (vcDatapathElement*) > Opfval;
    const map<Opfkey, Opfval> _opfmap = OPFMAP ;
    Ctyp wire2ctyp(vcWire* w)
    {
        assert(w);
        auto wiretyp = w->Get_Type();
        auto kind = wiretyp->Kind();
        auto width = wiretyp->Size();
        if ( kind == "vcArrayType" )
        {
            cout << "opfactory: Array type unexpected for wire" << endl;
            exit(1);
        }
        if ( wiretyp->Is_Int_Type() )
            return width <= 8 ? uint8_t__ : width <= 16 ? uint16_t__ :
                width <= 32 ? uint32_t__ : width <= 64 ? uint64_t__ : wuint__;
        if ( wiretyp->Is_Float_Type() )
        {
            if ( width == 32 ) return float__;
            if ( width == 64 ) return double__;
            cout << "opfactory: Unsupported floating point width " << width << endl;
            exit(1);
        }

        cout << "opfactory: Uknown wire type encounted" << endl;
        exit(1);
    }
    template <typename Opcls> Operator* create1(vcDatapathElement *dpe)
    {
        return new Opcls( dpe->Get_Output_Width(), dpe->Get_Id() );
    }
    template <typename Opcls> Operator* create2(vcDatapathElement *dpe)
    {
        return NULL;
    }
public:
    Operator* dpe2op(vcDatapathElement *dpe)
    {
        string kind = dpe->Kind();
        string vcid = ( kind == "vcBinarySplitOperator" or kind == "vcUnarySplitOperator" )
            ? ((vcSplitOperator*)dpe)->Get_Op_Id() : kind;
        vector<Ctyp> opsig;
        for (auto w:dpe->Get_Output_Wires()) opsig.push_back( wire2ctyp(w) );
        for (auto w:dpe->Get_Input_Wires()) opsig.push_back( wire2ctyp(w) );
        auto it = _opfmap.find( {vcid, opsig} );
        if ( it == _opfmap.end() )
        {
            cout << "Operator generator not found for vcid " << vcid << " signature";
            for(auto t:opsig) cout << " " << t;
            cout << endl;
            exit(1);
        }
        return it->second(dpe);
    }
};

#endif

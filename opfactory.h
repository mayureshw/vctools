#ifndef _OPFACTORY_H
#define _OPFACTORY_H

#include <tuple>
#include "opf.h"

class OpFactory
{
    SystemBase *_sys;
    typedef enum CTYPENUM Ctyp ;
    typedef pair< string, vector<Ctyp> > Opfkey;
    typedef function< Operator* (vcDatapathElement*) > Opfval;
    const map<Opfkey, Opfval> _opfmap = OPFMAP ;

    typedef enum PIFOENUM Pifotyp;
    typedef enum PBLKENUM Pblktyp;
    typedef tuple< Ctyp, Pifotyp, Pblktyp > Pfkey;
    typedef function< Pipe*(int, int, string) > Pfval;
    const map<Pfkey, Pfval> _pfmap = PFMAP;

    Ctyp width2ctyp(int width)
    {
        return width <= 8 ? uint8_t_ : width <= 16 ? uint16_t_ :
            width <= 32 ? uint32_t_ : width <= 64 ? uint64_t_ : wuint_;
    }
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
        if ( wiretyp->Is_Int_Type() ) return width2ctyp( width );
        if ( wiretyp->Is_Float_Type() )
        {
            if ( width == 32 ) return float_;
            if ( width == 64 ) return double_;
            cout << "opfactory: Unsupported floating point width " << width << endl;
            exit(1);
        }

        cout << "opfactory: Uknown wire type encounted" << endl;
        exit(1);
    }
    vector<DatumBase*>& getStoragev(vcDatapathElement *dpe)
    {
        auto objmap = ( (vcLoadStore*) dpe )->Get_Memory_Space()->Get_Object_Map();
        if ( objmap.size() != 1 )
        {
            cout << "vcLoadStore-memory_space-object_map size other than 1 not handled" << endl;
            exit(1);
        }
        vcStorageObject *sto;
        for(auto nsto:objmap) sto = nsto.second;
        return _sys->storageDatums(sto);
    }
    template <typename Opcls> Operator* createLoad(vcDatapathElement *dpe)
    {
        auto width = dpe->Get_Output_Width();
        return new Opcls( width, dpe->Get_Id(), getStoragev(dpe) );
    }
    template <typename Opcls> Operator* createStore(vcDatapathElement *dpe)
    {
        if ( dpe->Get_Input_Wires().size() != 2 )
        {
            cout << "Store operator with input wires count !=2" << endl;
            exit(1);
        }
        auto width = dpe->Get_Input_Wires()[1]->Get_Size();
        return new Opcls( width, dpe->Get_Id(), getStoragev(dpe) );
    }
    template <typename Opcls> Operator* createIOPort(vcDatapathElement *dpe)
    {
        vcPipe* vcpipe = ((vcIOport*)dpe)->Get_Pipe();
        Pipe* pipe = _sys->pipeMap(vcpipe);
        auto width = vcpipe->Get_Width();
        return new Opcls( dpe->Get_Output_Width(), dpe->Get_Id(), pipe);
    }
    template <typename Opcls> Operator* createCall(vcDatapathElement *dpe)
    {
        auto calledVcModule = ((vcCall*)dpe)->Get_Called_Module();
        auto cm = _sys->getModule( calledVcModule );
        auto ipv = cm->iparamV();
        auto opv = cm->oparamV();
        return new Call(ipv, opv, dpe->Get_Id());
    }
    template <typename Opcls> Operator* createSlice(vcDatapathElement *dpe)
    {
        auto vcslice = (vcSlice*) dpe;
        int width = vcslice->Get_Output_Width();
        int h = vcslice->Get_High_Index();
        int l = vcslice->Get_Low_Index();
        return new Opcls(width, dpe->Get_Id(), h, l);
    }
    template <typename Opcls> Operator* createGeneral(vcDatapathElement *dpe)
    {
        return new Opcls( dpe->Get_Output_Width(), dpe->Get_Id() );
    }
public:
    Operator* dpe2op(vcDatapathElement *dpe)
    {
        string kind = dpe->Kind();
        string vcid = ( kind == "vcBinarySplitOperator" or kind == "vcUnarySplitOperator" )
            ? ((vcSplitOperator*)dpe)->Get_Op_Id() : kind;
        vector<Ctyp> opsig;
        if ( kind != "vcCall" )
        {
            for (auto w:dpe->Get_Output_Wires()) opsig.push_back( wire2ctyp(w) );
            for (auto w:dpe->Get_Input_Wires()) opsig.push_back( wire2ctyp(w) );
        }
        auto it = _opfmap.find( {vcid, opsig} );
        if ( it == _opfmap.end() )
        {
            cout << "Operator generator not found for element " << dpe->Get_Id() << " vcid " << vcid << " signature";
            for(auto t:opsig) cout << " " << t;
            cout << endl;
            exit(1);
        }
        return it->second(dpe);
    }
    Pipe* vcp2p(vcPipe *vcp)
    {
        auto ctyp = width2ctyp( vcp->Get_Width() );
        auto pifo = vcp->Get_Lifo_Mode() ? Lifo_ : Fifo_;
        auto pblk = vcp->Get_Signal() ? SignalPipe_ : vcp->Get_No_Block_Mode() ? NonBlockingPipe_ : BlockingPipe_;

        auto it = _pfmap.find( { ctyp, pifo, pblk } );
        if ( it == _pfmap.end() )
        {
            cout << "Pipe generator not found for pipe " << vcp->Get_Id() << " signature " << ctyp << " " << pifo << " " << pblk << endl;
        }
        return it->second( vcp->Get_Depth(), vcp->Get_Width(), vcp->Get_Id() );
    }
    OpFactory(SystemBase* sys) : _sys(sys) {}
};

#endif

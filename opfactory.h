#ifndef _OPFACTORY_H
#define _OPFACTORY_H

#include <tuple>
#include "opf.h"
#ifdef USECEP
#include "stateif.h"
#endif

#define CTYPSWITCH( CTYP, FN, ... ) \
    switch ( CTYP ) \
    { \
        case double_   : return FN<double  > ( __VA_ARGS__ ); \
        case float_    : return FN<float   > ( __VA_ARGS__ ); \
        case uint16_t_ : return FN<uint16_t> ( __VA_ARGS__ ); \
        case uint32_t_ : return FN<uint32_t> ( __VA_ARGS__ ); \
        case uint64_t_ : return FN<uint64_t> ( __VA_ARGS__ ); \
        case uint8_t_  : return FN<uint8_t > ( __VA_ARGS__ ); \
        case WUINT_    : return FN<WUINT   > ( __VA_ARGS__ ); \
        default : \
            cout << "Encountered unknown type " << CTYP << endl; \
            exit(1); \
    }

#ifdef USECEP
class OpFactory : public CEPStateIf
#else
class OpFactory
#endif
{
#ifdef USECEP
    map<int,Operator*> _opidmap;
    DatumBase* stateidv2datum(vector<int>& idv)
    {
        if ( idv.size() != 2 )
        {
            cout << "State index vector size !=2 not supported" << endl;
            exit(1);
        }
        auto dpeid = idv[0];
        auto opidx = idv[1];
        auto it = _opidmap.find(dpeid);
        if ( it == _opidmap.end() )
        {
            cout << "No operator for DPEId " << dpeid << endl;
            exit(1);
        }
        auto opv = it->second->opv;
        if ( opv.size() < opidx + 1 )
        {
            cout << "Operator " << it->second->_dplabel << " output arity=" << opv.size() << " Sought index=" << opidx << endl;
            exit(1);
        }
        return opv[ opidx ];
    }
#endif
    SystemBase *_sys;
    typedef enum CTYPENUM Ctyp ;
    typedef pair< string, vector<Ctyp> > Opfkey;
    typedef function< Operator* (vcDatapathElement*) > Opfval;
    const map<Opfkey, Opfval> _opfmap = OPFMAP ;

    typedef enum PIFOENUM Pifotyp;
    typedef enum PBLKENUM Pblktyp;
    typedef tuple< Ctyp, Pifotyp, Pblktyp > Pfkey;
    typedef function< Pipe*(int, int, string, VcPetriNet*) > Pfval;
    const map<Pfkey, Pfval> _pfmap = PFMAP;

    internmap<vcValue*,DatumBase*> _valueDatum {bind(&OpFactory::vcv2datum,this,_1)};
    map<vcStorageObject*,vector<DatumBase*>> _storageDatums;

    Ctyp width2ctyp(int width)
    {
        if ( width > WIDEUINTSZ )
        {
            cout << "Wide int size (configurable) limit is: " << WIDEUINTSZ << " sought " << width << endl;
            exit(1);
        }
        return width <= 8 ? uint8_t_ : width <= 16 ? uint16_t_ :
            width <= 32 ? uint32_t_ : width <= 64 ? uint64_t_ : WUINT_;
    }
    Ctyp vctyp2ctyp(vcType* vct)
    {
        if ( vct->Kind() == "vcArrayType" )
        {
            cout << "opfactory: Array type unexpected in vctyp2ctyp" << endl;
            exit(1);
        }
        auto width = vct->Size();
        if ( vct->Is_Int_Type() ) return width2ctyp( width );
        if ( vct->Is_Float_Type() )
        {
            if ( width == 32 ) return float_;
            if ( width == 64 ) return double_;
            cout << "opfactory: Unsupported floating point width " << width << endl;
            exit(1);
        }
        cout << "opfactory: Uknown type encounted" << endl;
        exit(1);
    }
    vector<DatumBase*>& getStoragev(vcDatapathElement *dpe)
    {
        auto sto = getStorageObj(dpe);
        auto it = _storageDatums.find(sto);
        if ( it == _storageDatums.end() )
        {
            _storageDatums.emplace(sto, sto2datumv(sto));
            return _storageDatums[sto];
        }
        return it->second;
    }
    template <typename Opcls> Operator* createLoad(vcDatapathElement *dpe)
    {
        auto width = dpe->Get_Output_Width();
        return new Opcls( width, dpe->Get_Id(), getStoragev( dpe ) );
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
        int l = vcslice->Get_Low_Index();
        return new Opcls(width, dpe->Get_Id(), l);
    }
    template <typename Opcls> Operator* createGeneral(vcDatapathElement *dpe)
    {
        return new Opcls( dpe->Get_Output_Width(), dpe->Get_Id() );
    }
    template <typename Opcls> Operator* createCast(vcDatapathElement *dpe)
    {
        auto ipwires = dpe->Get_Input_Wires();
        if ( ipwires.size() != 1 )
        {
            cout << "Cast operator input wire count expected: 1, got"
                << ipwires.size() << endl;
            exit(1);
        }
        return new Opcls( dpe->Get_Output_Width(), ipwires[0]->Get_Size(), dpe->Get_Id() );
    }
public:
    vcStorageObject* getStorageObj(vcDatapathElement* dpe)
    {
        auto objmap = ( (vcLoadStore*) dpe )->Get_Memory_Space()->Get_Object_Map();
        if ( objmap.size() != 1 )
        {
            cout << "vcLoadStore-memory_space-object_map size other than 1 not handled" << endl;
            exit(1);
        }
        vcStorageObject *sto;
        for(auto nsto:objmap) sto = nsto.second;
        return sto;
    }
#ifdef USECEP
    void* getStatePtr(vector<int>& idv) { return stateidv2datum(idv)->elemPtr(); }
    Etyp getStateTyp(vector<int>& idv) { return stateidv2datum(idv)->etyp(); }
    void quit() { _sys->stop(); }
#endif
    Operator* dpe2op(vcDatapathElement *dpe)
    {
        string kind = dpe->Kind();
        string vcid = ( kind == "vcBinarySplitOperator" or kind == "vcUnarySplitOperator" )
            ? ((vcSplitOperator*)dpe)->Get_Op_Id() : kind;
        vector<Ctyp> opsig;
        if ( kind != "vcCall" )
        {
            for (auto w:dpe->Get_Output_Wires()) opsig.push_back( vctyp2ctyp(w->Get_Type()) );
            for (auto w:dpe->Get_Input_Wires()) opsig.push_back( vctyp2ctyp(w->Get_Type()) );
        }
        auto it = _opfmap.find( {vcid, opsig} );
        if ( it == _opfmap.end() )
        {
            cout << "Operator generator not found for element " << dpe->Get_Id() << " kind " << kind << " vcid " << vcid << " signature";
            for(auto t:opsig) cout << " " << t;
            cout << endl;
            exit(1);
        }
        auto op = it->second(dpe);
#ifdef USECEP
        _opidmap.emplace( dpe->Get_Root_Index(), op );
#endif
        return op;
    }
    Pipe* vcp2p(vcPipe *vcp, VcPetriNet* pn)
    {
        auto ctyp = width2ctyp( vcp->Get_Width() );
        auto pifo = vcp->Get_Lifo_Mode() ? Lifo_ : Fifo_;
        auto pblk = vcp->Get_Signal() ? SignalPipe_ : vcp->Get_No_Block_Mode() ? NonBlockingPipe_ : BlockingPipe_;

        auto it = _pfmap.find( { ctyp, pifo, pblk } );
        if ( it == _pfmap.end() )
        {
            cout << "Pipe generator not found for pipe " << vcp->Get_Id() << " signature " << ctyp << " " << pifo << " " << pblk << endl;
        }
        // TODO: For strange reasons, we get depth as 2 from the parser level
        // For signals AHIR semantics require that the depth be restricted to 1
        auto depth = pblk == SignalPipe_ ? 1 : vcp->Get_Depth();
        return it->second( depth, vcp->Get_Width(), vcp->Get_Id(), pn );
    }
    template < typename T > DatumBase* createDatum(unsigned width)
    {
        return new Datum<T>( width );
    }
    template < typename T > void createDatums(unsigned width, unsigned dim, vector<DatumBase*>& datums)
    {
        for(int i=0; i<dim; i++) datums.push_back( new Datum<T>(width) );
    }
    DatumBase* vct2datum(vcType* vct)
    {
        auto ctyp  = vctyp2ctyp( vct );
        auto width = vct->Size();
        CTYPSWITCH( ctyp, createDatum, width );
    }
    void vct2datums(vcType* vct, unsigned dim, vector<DatumBase*>& datums)
    {
        auto ctyp  = vctyp2ctyp( vct );
        auto width = vct->Size();
        CTYPSWITCH( ctyp, createDatums, width, dim, datums );
    }
    DatumBase* vcv2datum(vcValue* vcval)
    {
        auto valtyp = vcval->Get_Type();
        string val;
        if ( valtyp->Is_Int_Type() ) val = ((vcIntValue*)vcval)->Get_Value();
        else if ( valtyp->Is_Float_Type() ) val = ((vcFloatValue*)vcval)->Get_Value();
        else
        {
            cout << "Unhandled value type " << valtyp->Kind() << endl;
            exit(1);
        }
        auto retdat = vct2datum( valtyp );
        *retdat = val;
        return retdat;
    }
    DatumBase* valueDatum(vcValue* vcval) { return _valueDatum[vcval]; }
    vector<DatumBase*> sto2datumv(vcStorageObject* sto)
    {
        auto stoTyp = sto->Get_Type();
        int dim;
        vcType* elemTyp;
        if ( stoTyp->Kind() == "vcArrayType" )
        {
            auto arrayTyp = (vcArrayType*) stoTyp;
            dim =  arrayTyp->Get_Dimension();
            elemTyp = arrayTyp->Get_Element_Type();
        }
        else
        {
            dim = 1;
            elemTyp = stoTyp;
        }
        vector<DatumBase*> retdat;
        vct2datums(elemTyp, dim, retdat);
        return retdat;
    }
    OpFactory(SystemBase* sys) : _sys(sys) {}
    ~OpFactory()
    {
        for(auto d:_valueDatum.getmap()) delete d.second;
        for(auto dv:_storageDatums) for(auto d:dv.second) delete d;
    }
};

#endif

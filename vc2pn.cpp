#include <iostream>
#include "vc2pn.h"
#include "vcsim.h"


using namespace std;

PETRINET_STATICS

#define VCMAP(T) { #T, T##_ },
const map<string,Vctyp> DPElement::vctypmap =
    {
        VCMAP(vcInport)
        VCMAP(vcOutport)
        VCMAP(vcBranch)
        VCMAP(vcPhi)
        VCMAP(vcPhiPipelined)
        VCMAP(vcIOport)
        VCMAP(vcBinarySplitOperator)
        VCMAP(vcUnarySplitOperator)
        VCMAP(vcInterlockBuffer)
        VCMAP(vcEquivalence)
        VCMAP(vcLoad)
        VCMAP(vcStore)
        VCMAP(vcCall)
        VCMAP(vcSelect)
        VCMAP(vcSlice)
    };

const map<string,Optyp> DPElement::opidoptmap =
    {
        {"+",plus_},
        {"-",minus_},
        {"*",mult_},
        {"<",lt_},
        {">",gt_},
        {">=",ge_},
        {"!=",ne_},
        {"==",eq_},
        {"&",and_},
        {"|",or_},
        {"~",not_},
        {"[]",bitsel_},
        {"&&",concat_},
    };

const map<Vctyp,Optyp> DPElement::vctoptmap =
    {
        {vcInterlockBuffer_,assign_},
        {vcPhi_,phi_},
        {vcPhiPipelined_,phi_},
        {vcBranch_,branch_},
        {vcEquivalence_,equiv_},
        {vcLoad_,load_},
        {vcStore_,store_},
        {vcInport_,inport_},
        {vcOutport_,outport_},
        {vcCall_,call_},
        {vcSelect_,select_},
        {vcSlice_,slice_},
    };

// A few global methods for entities that we didn't model in vc2pn layer
Wiretyp vctWiretyp(vcType* vct)
{
    vcType* vcEtyp = ( vct->Kind() == "vcArrayType" ) ?
        ((vcArrayType*)vct)->Get_Element_Type() : vct;
    if ( vcEtyp->Is_Int_Type() ) return int_;
    else if ( vcEtyp->Is_Float_Type() ) return float_;
    else
    {
        cout << "vc2pn: Unhandled vc type" << endl;
        exit(1);
    }
}
unsigned vctDim(vcType* vct)
{
    return ( vct->Kind() == "vcArrayType" ) ? ((vcArrayType*)vct)->Get_Dimension() : 1;
}

unsigned vctElemWidth(vcType* vct)
{
    return ( vct->Kind() == "vcArrayType" ) ? ((vcArrayType*)vct)->Get_Element_Type()->Size() : vct->Size();
}

#define TRYTYPD(CTYP) width <= (sizeof(CTYP) << 3) ? (DatumBase*) new Datum<CTYP>(width)
#define INTDATUM TRYTYPD(unsigned char) : TRYTYPD(unsigned short) : TRYTYPD(unsigned int) : (DatumBase*) new Datum<unsigned long>(width)
DatumBase* vct2datum(vcType* vct)
{
    DatumBase* retdat;
    Wiretyp wtyp = vctWiretyp(vct);
    unsigned width = vctElemWidth(vct);
    switch(wtyp)
    {
        case int_:
            retdat = INTDATUM;
            break;
        case float_:
            retdat = new Datum<float>(width);
            break;
        default:
            cout << "vc2pn: unhandled type in vc2datum: " << wtyp << endl;
            exit(1);
    }
    return retdat;
}
void vct2datums(vcType* vct, unsigned dim, vector<DatumBase*>& datums)
{
    Wiretyp wtyp = vctWiretyp(vct);
    unsigned width = vctElemWidth(vct);
    switch(wtyp)
    {
        case int_:
            for(int i=0; i<dim; i++) datums.push_back(INTDATUM);
            break;
        case float_:
            for(int i=0; i<dim; i++) datums.push_back(new Datum<float>(width));
            break;
        default:
            cout << "vc2pn: unhandled type in vc2datum: " << wtyp << endl;
            exit(1);
    }
}

vector<Wiretyp> vcWiresTypes(vector<vcWire*>& wires)
{
    vector<Wiretyp> retv;
    for(auto w:wires) retv.push_back( vctWiretyp(w->Get_Type()) );
    return retv;
}

// TODO: To be extended for multiple files, exit module, top module etc.
// Currently accepts only single vc filename and invokation modulename
// besides input vector
void vcsim(const string vcflnm, const string invoke, const vector<DatumBase*>& inpv,
    const vector<pair<string,vector<DatumBase*>>>& feeds,
    const vector<pair<string,unsigned>>& collects,
    const set<string>& daemons,
    map<string,vector<DatumBase*>>& collectopmap
    )
{
    ifstream stream;
    stream.open(vcflnm);
    vcSystem::_opt_flag = true;

    vcSystem vcs("sys");
    vcs.Parse(vcflnm);
    // Setting all modules as top. Will have to borrow one or more CLI args of
    // vc2vhdl. It's mainly for vc IR to clean up unreachable modules, so
    // doesn't affect much to the simulator.
    for(auto m:vcs.Get_Modules()) vcs.Set_As_Top_Module(m.second);
    vcs.Elaborate();
    vcs.Print_Reduced_Control_Paths_As_Dot_Files();

    System sys(&vcs, daemons);
    sys.printDPDotFiles();
    sys.printPNDotFile();
    sys.printPNJsonFile();
    sys.printPNPNMLFile();
    sys.invoke(invoke, inpv, feeds, collects, collectopmap);
}

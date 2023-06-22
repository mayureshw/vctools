using namespace std;

#include <iostream>
#include <functional>
#include <filesystem>
#include "vcLexer.hpp"
#include "vcParser.hpp"
#include "cpp2xsb.h"
#include "vc2pn.h"

class ModuleIR
{
    vcModule* _vcm;
    ModuleBase* _simmod;
    System& _sys;
    Rel<long,string> _cpe = {"cpe"};
    Rel<long,string,string,long> _dpe = {"dpe"};
    Rel<long,long> _cpeg = {"cpeg"};
    Rel<long,long,long> _dpdep = {"dpdep"};
    Rel<long,string> _dppipe = {"dppipe"};
    Rel<long,string> _dpstore = {"dpstore"};
    void processCPE()
    {
        auto cp = _vcm->Get_Control_Path();
        auto egmap = cp->Get_CPElement_To_Group_Map();
        for(auto egpair:egmap)
        {
            _cpe.add({
                egpair.first->Get_Root_Index(),
                egpair.first->Get_Id(),
                });
            if ( egpair.second ) _cpeg.add({
                egpair.first->Get_Root_Index(),
                egpair.second->Get_Root_Index()
                });
        }
    }
    void processDPED(vcDatapathElement* dpe)
    {
        auto iws = dpe->Get_Input_Wires();
        for(int i=0; i<iws.size(); i++)
            if ( iws[i]->Get_Driver() )
                _dpdep.add({
                    dpe->Get_Root_Index(),
                    i,
                    iws[i]->Get_Driver()->Get_Root_Index(),
                    });
    }
    void processDPPipeStores(vcDatapathElement* dpe)
    {
        if ( dpe->Kind() == "vcInport" or dpe->Kind() == "vcOutport" )
        {
            auto pipename = ((vcIOport*) dpe)->Get_Pipe()->Get_Id();
            _dppipe.add({ dpe->Get_Root_Index(), pipename });
        }
        else if ( dpe->Kind() == "vcLoad" or dpe->Kind() == "vcStore" )
        {
            auto storename = _sys.getStorageObj((vcLoadStore*)dpe)->Get_Id();
            _dpstore.add({ dpe->Get_Root_Index(), storename });
        }
    }
    void processDPE()
    {
        for(auto dpet:_vcm->Get_Data_Path()->Get_DPE_Map())
        {
            _dpe.add({
                dpet.second->Get_Root_Index(),
                dpet.first,
                dpet.second->Kind(),
                _vcm->Get_Root_Index()
                });
            processDPPipeStores(dpet.second);
            processDPED(dpet.second);
        }
    }
    JsonList* pnv2jsonlist(JsonFactory& jf, vector<PNTransition*> pnv)
    {
        auto l = jf.createJsonList();
        for(auto pn:pnv)
            l->push_back( jf.createJsonAtom<unsigned>(pn->_nodeid) );
        return l;
    }
public:
    PNPlace* entryPlace() { return _simmod->entryPlace(); }
    PNPlace* exitPlace() { return _simmod->exitPlace(); }
    string name() { return _simmod->name(); }
    void export_prolog(ofstream& pfile)
    {
        _cpeg.dump(pfile);
        _cpe.dump(pfile);
        _dpe.dump(pfile);
        _dpdep.dump(pfile);
        _dppipe.dump(pfile);
        _dpstore.dump(pfile);
    }
    void buildJsonDPEIOWidths(JsonFactory& jf, vector<vcWire*>& iws, vector<vcWire*>& ows, JsonMap* dpedict)
    {
        auto iwidths_key = jf.createJsonAtom<string>("iwidths");
        auto owidths_key = jf.createJsonAtom<string>("owidths");
        auto iwidthslist = jf.createJsonList();
        auto owidthslist = jf.createJsonList();
        dpedict->push_back({ iwidths_key, iwidthslist });
        dpedict->push_back({ owidths_key, owidthslist });
        for(auto iw:iws)
        {
            auto width_val = jf.createJsonAtom<unsigned>( iw->Get_Size() );
            iwidthslist->push_back( width_val );
        }
        for(auto ow:ows)
        {
            auto width_val = jf.createJsonAtom<unsigned>( ow->Get_Size() );
            owidthslist->push_back( width_val );
        }
    }
    void buildJsonDPEInputs(JsonFactory& jf, vector<vcWire*>& iws, JsonMap* dpedict)
    {
        auto dpinps_key = jf.createJsonAtom<string>("dpinps");
        auto fpinps_key = jf.createJsonAtom<string>("fpinps");
        auto constinps_key = jf.createJsonAtom<string>("constinps");
        auto id_key = jf.createJsonAtom<string>("id");
        auto oppos_key = jf.createJsonAtom<string>("oppos");

        auto dpinpsdict = jf.createJsonMap();
        dpedict->push_back({ dpinps_key, dpinpsdict});
        auto constinpdict = jf.createJsonMap();
        dpedict->push_back({ constinps_key, constinpdict });
        auto fpinpdict = jf.createJsonMap();
        dpedict->push_back({ fpinps_key, fpinpdict });

        for(int i=0; i<iws.size(); i++)
        {
            auto w = iws[i];
            auto i_val = jf.createJsonAtom<string>( to_string(i) );
            auto driver = w->Get_Driver();
            if ( driver )
            {
                auto dpinpssubdict = jf.createJsonMap();
                dpinpsdict->push_back({ i_val, dpinpssubdict });

                auto inpid_val = jf.createJsonAtom<unsigned>( driver->Get_Root_Index() );
                dpinpssubdict->push_back({id_key, inpid_val});

                vector<int> opindices;
                driver->Get_Output_Wire_Indices(w, opindices);
                assert(opindices.size() == 1);
                auto oppos_val = jf.createJsonAtom<unsigned>(opindices[0]);
                dpinpssubdict->push_back({oppos_key, oppos_val});
            }
            else if ( w->Is_Constant() )
            {
                auto vcv = ( (vcConstantWire*) w )->Get_Value();
                string const_str = _sys.valueDatum( vcv )->str();
                auto const_val = jf.createJsonAtom<string>(const_str);
                constinpdict->push_back({ i_val, const_val });
            }
            else if ( w->Kind() == "vcInputWire" )
            {
                auto fpinpssubdict = jf.createJsonMap();
                fpinpdict->push_back({ i_val, fpinpssubdict });

                auto fpid_val = jf.createJsonAtom<unsigned>( entryPlace()->_nodeid );
                fpinpssubdict->push_back({id_key, fpid_val});

                auto parampos = _simmod->getInpParamPos( w->Get_Id() );
                auto oppos_val = jf.createJsonAtom<unsigned>(parampos);
                fpinpssubdict->push_back({oppos_key, oppos_val});
            }
            else
            {
                cout << "vcexport: Unhandled wire type " << w->Kind() << endl;
                exit(1);
            }
        }
    }
    void buildModuleJsonEntry(JsonFactory& jf, JsonMap* modulesmap)
    {
        auto exit_key = jf.createJsonAtom<string>("exit");
        auto entryPlace_key = jf.createJsonAtom<string>( to_string( entryPlace()->_nodeid ) );

        auto moduleDict = jf.createJsonMap();
        modulesmap->push_back({ entryPlace_key, moduleDict });

        auto exit_val = jf.createJsonAtom<unsigned>( exitPlace()->_nodeid );
        moduleDict->push_back( { exit_key, exit_val } );

        vector<vcWire*> iws, ows;
        for( int i=0; i<_vcm->Get_Number_Of_Input_Arguments(); i++)
            iws.push_back( _vcm->Get_Input_Wire(i) );
        for( int i=0; i<_vcm->Get_Number_Of_Output_Arguments(); i++)
            ows.push_back( _vcm->Get_Output_Wire(i) );
        buildJsonDPEIOWidths(jf, iws, ows, moduleDict);
        // Note: we are exporting inputs to the exitPlace, hence we pass ows
        buildJsonDPEInputs(jf, ows, moduleDict);
    }
    void buildJsonDPEList(JsonFactory& jf, JsonMap* dpesmap, JsonMap* modulesmap)
    {
        buildModuleJsonEntry(jf, modulesmap);

        auto label_key = jf.createJsonAtom<string>("label");
        auto optyp_key = jf.createJsonAtom<string>("optyp");
        auto reqs_key = jf.createJsonAtom<string>("reqs");
        auto greqs_key = jf.createJsonAtom<string>("greqs");
        auto acks_key = jf.createJsonAtom<string>("acks");
        auto gacks_key = jf.createJsonAtom<string>("gacks");
        auto ftreq_key = jf.createJsonAtom<string>("ftreq");
        auto ftack_key = jf.createJsonAtom<string>("ftack");
        auto callentry_key = jf.createJsonAtom<string>("callentry");
        auto callexit_key = jf.createJsonAtom<string>("callexit");
        auto callack_key = jf.createJsonAtom<string>("callack");

        for( auto simdpe : _simmod->getDPEList() )
        {
            auto dpeid = simdpe->elem()->Get_Root_Index();
            auto id_val  = jf.createJsonAtom<string>(to_string(dpeid));

            auto dpedict = jf.createJsonMap();
            dpesmap->push_back({id_val,dpedict});

            auto label_val = jf.createJsonAtom<string>( simdpe->label() );
            dpedict->push_back({ label_key, label_val });

            auto op = simdpe->getOp();
            auto optyp_val = jf.createJsonAtom<string>(op->oplabel());
            dpedict->push_back({optyp_key,optyp_val});

            dpedict->push_back({ reqs_key, pnv2jsonlist(jf, simdpe->getReqs())});
            dpedict->push_back({ greqs_key, pnv2jsonlist(jf, simdpe->getGReqs())});
            dpedict->push_back({ acks_key, pnv2jsonlist(jf, simdpe->getAcks())});
            dpedict->push_back({ gacks_key, pnv2jsonlist(jf, simdpe->getGAcks())});
            vector<PNTransition*> ftreqv, ftackv;
            if ( simdpe->isDeemedFlowThrough() )
            {
                ftreqv.push_back( simdpe->ftreq() );
                ftackv.push_back( simdpe->ftack() );
            }
            dpedict->push_back( { ftreq_key, pnv2jsonlist(jf, ftreqv) } );
            dpedict->push_back( { ftack_key, pnv2jsonlist(jf, ftackv) } );

            if ( simdpe->isCall() )
            {
                auto calledVcModule = ((vcCall*)simdpe->elem())->Get_Called_Module();
                auto calledModule = _sys.getModule( calledVcModule );

                auto calledEntryPlaceId = calledModule->entryPlace()->_nodeid;
                auto calledEntryPlace_val = jf.createJsonAtom<unsigned>( calledEntryPlaceId );
                dpedict->push_back({ callentry_key, calledEntryPlace_val });

                auto calledExitPlaceId = calledModule->exitPlace()->_nodeid;
                auto calledExitPlace_val = jf.createJsonAtom<unsigned>( calledExitPlaceId );
                dpedict->push_back({ callexit_key, calledExitPlace_val });

                auto callUackId = simdpe->getAckTransition(1)->_nodeid;
                auto callUackId_val = jf.createJsonAtom<unsigned>( callUackId );
                dpedict->push_back({ callack_key, callUackId_val });
            }

            auto iws = simdpe->elem()->Get_Input_Wires();
            auto ows = simdpe->elem()->Get_Output_Wires();
            buildJsonDPEIOWidths(jf, iws, ows, dpedict);
            buildJsonDPEInputs(jf, iws, dpedict);
        }
    }
    ModuleIR(vcModule* vcm, System& sys) : _vcm(vcm), _sys(sys), _simmod(sys.getModule(vcm))
    {
        // processCPE(); // Disabled till needed
        processDPE();
    }
};

class SysIR
{
    list<ModuleIR*> _moduleirs;
    Rel<long,string> _m = {"m"};
    System& _sys;
public:
    void export_prolog()
    {
        string filename = _sys.name() + ".vcir";
        ofstream pfile(filename);
        _m.dump(pfile);
        for(auto mir:_moduleirs) mir->export_prolog(pfile);
        _sys.pn()->vctid.dump(pfile);
        pfile.close();
    }
    void export_json()
    {
        // 1. Petri net export (generic, unaware of vC)
        string pnetflnm = _sys.name() + "_petri.json";
        ofstream pnetfile(pnetflnm);
        _sys.pn()->printjson(pnetfile);
        pnetfile.close();

        string jsonflnm = _sys.name() + ".json";
        ofstream jsonfile(jsonflnm);

        // 2. Factory and top object declaration
        JsonFactory jf;
        JsonMap top;

        JSONSTR(mutexes)
        JSONSTR(passive_branches)
        JSONSTR(branches)
        JSONSTR(simu_only)

        list<pair<JsonKey*,PNAnnotation>> annkeys {
            { &mutexes_key,          Mutex_         },
            { &passive_branches_key, PassiveBranch_ },
            { &branches_key,         Branch_        },
            { &simu_only_key,        SimuOnly_      },
            };

        // 3. Additional vC specific properties for Petri net
        for(auto p:annkeys)
        {
            auto annlist = jf.createJsonList();
            for(auto n:_sys.pn()->getAnnotatedNodeset(p.second))
                annlist->push_back( jf.createJsonAtom<unsigned>( n->_nodeid ) );
            top.push_back( { p.first, annlist } );
        }

        // 4. vC module info
        JSONSTR(modules)
        auto modulesmap = jf.createJsonMap();
        top.push_back( { &modules_key, modulesmap } );

        // 5. vC data path
        JSONSTR(dpes)
        auto dpesmap = jf.createJsonMap();
        top.push_back( { &dpes_key, dpesmap } );
        for(auto mir:_moduleirs) mir->buildJsonDPEList(jf, dpesmap, modulesmap);

        // 6. Write json file
        top.print(jsonfile);
        jsonfile.close();
    }
    SysIR(vcSystem& vcs, System& sys) : _sys(sys)
    {
        for(auto m:vcs.Get_Ordered_Modules())
        {
            _moduleirs.push_back(new ModuleIR(m,_sys));
            _m.add({ m->Get_Root_Index(), m->Get_Id() });
        }
    }
    ~SysIR()
    {
        for(auto mir:_moduleirs) delete mir;
    }
};

#define EXPORTFN(FORMAT) \
{ #FORMAT, [](SysIR& sysir) { sysir.export_##FORMAT(); } }

map<string,function<void(SysIR&)>> exporters =
{
    EXPORTFN(prolog),
    EXPORTFN(json),
};

void usage(char* argv0)
{
    string formatopts = "";
    for(auto expfn : exporters)
    {
        if ( formatopts != "" ) formatopts += "|";
        formatopts += expfn.first;
    }
    cout << "Usage: " << argv0 << " <" << formatopts << "> <vcfile> [daemons...]" << endl;
    exit(1);
}

int main(int argc, char* argv[])
{

    if (argc < 3) usage(argv[0]);

    string vcflnm = argv[2];
    filesystem::path vcpath(vcflnm);
    string basename = vcpath.stem();

    auto it = exporters.find(argv[1]);
    if ( it == exporters.end() ) usage(argv[0]);

    auto exportfn = it->second;

    set<string> daemons;
    for(int i=3; i<argc; i++) daemons.insert(argv[i]);

    vcSystem::_opt_flag = true;
    vcSystem vcs(basename);
    vcs.Parse(vcflnm);
    // Setting all modules as top. Will have to borrow one or more CLI args of
    // vc2vhdl. It's mainly for vc IR to clean up unreachable modules, so
    // doesn't affect much to the simulator.
    for(auto m:vcs.Get_Modules()) vcs.Set_As_Top_Module(m.second);
    vcs.Elaborate();
    System sys(&vcs, daemons);

    SysIR sysir(vcs,sys);
    exportfn(sysir);
}

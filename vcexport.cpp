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
public:
    void export_prolog(ofstream& pfile)
    {
        _cpeg.dump(pfile);
        _cpe.dump(pfile);
        _dpe.dump(pfile);
        _dpdep.dump(pfile);
        _dppipe.dump(pfile);
        _dpstore.dump(pfile);
    }
    void buildJsonDPEList(JsonFactory& jf, JsonList* dpelist)
    {
        auto simmodule = _sys.getModule(_vcm);
        auto id_key = jf.createJsonAtom<string>("id");
        auto optyp_key = jf.createJsonAtom<string>("optyp");
        auto opwidth_key = jf.createJsonAtom<string>("opwidth");

        for( auto simdpe : simmodule->getDPEList() )
        {
            auto dpedict = jf.createJsonMap();
            dpelist->push_back(dpedict);

            auto dpeid = simdpe->elem()->Get_Root_Index();
            auto id_val  = jf.createJsonAtom<unsigned>(dpeid);
            dpedict->push_back({id_key,id_val});

            auto op = simdpe->getOp();

            auto optyp_val = jf.createJsonAtom<string>(op->oplabel());
            dpedict->push_back({optyp_key,optyp_val});

            auto opwidth_val = jf.createJsonAtom<unsigned>(op->opwidth());
            dpedict->push_back({opwidth_key,opwidth_val});
        }
    }
    ModuleIR(vcModule* vcm, System& sys) : _vcm(vcm), _sys(sys)
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

        list<pair<JsonKey*,PNAnnotation>> annkeys {
            { &mutexes_key,          Mutex_         },
            { &passive_branches_key, PassiveBranch_ },
            { &branches_key,         Branch_        },
            };

        // 3. Additional vC specific properties for Petri net
        for(auto p:annkeys)
        {
            auto annlist = jf.createJsonList();
            for(auto n:_sys.pn()->getNodeset(p.second))
                annlist->push_back( jf.createJsonAtom<unsigned>( n->_nodeid ) );
            top.push_back( { p.first, annlist } );
        }

        // 4. vC data path
        JSONSTR(dpes)
        auto dpelist = jf.createJsonList();
        top.push_back( { &dpes_key, dpelist } );
        for(auto mir:_moduleirs) mir->buildJsonDPEList(jf, dpelist);

        // 5. Write json file
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

using namespace std;

#include <iostream>
#include "vcLexer.hpp"
#include "vcParser.hpp"
#include "cpp2xsb.h"
#include "vc2pn.h"

class ModuleIR
{
    vcModule* _vcm;
    Rel<long,string> _cpe = {"cpe"};
    Rel<long,string,string> _dpe = {"dpe"};
    Rel<long,long> _cpeg = {"cpeg"};
    Rel<long,long,long> _dpdep = {"dpdep"};
    Rel<string,long> _dppipe = {"dppipe"};
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
    void processDPPipe(vcDatapathElement* dpe)
    {
        if ( dpe->Kind() == "vcInport" or dpe->Kind() == "vcOutport" )
        {
            auto pipename = ((vcIOport*) dpe)->Get_Pipe()->Get_Id();
            _dppipe.add({ pipename, dpe->Get_Root_Index() });
        }
    }
    void processDPE()
    {
        for(auto dpet:_vcm->Get_Data_Path()->Get_DPE_Map())
        {
            _dpe.add({
                dpet.second->Get_Root_Index(),
                dpet.first,
                dpet.second->Kind()
                });
            processDPPipe(dpet.second);
            processDPED(dpet.second);
        }
    }
public:
    void exportIR(ofstream& pfile)
    {
        _cpeg.dump(pfile);
        _cpe.dump(pfile);
        _dpe.dump(pfile);
        _dpdep.dump(pfile);
        _dppipe.dump(pfile);
    }
    ModuleIR(vcModule* vcm) : _vcm(vcm)
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
    void exportIR(ofstream& pfile)
    {
        _m.dump(pfile);
        for(auto mir:_moduleirs) mir->exportIR(pfile);
        _sys.pn()->vctid.dump(pfile);
    }
    SysIR(vcSystem& vcs, System& sys) : _sys(sys)
    {
        for(auto m:vcs.Get_Ordered_Modules())
        {
            _moduleirs.push_back(new ModuleIR(m));
            _m.add({ m->Get_Root_Index(), m->Get_Id() });
        }
    }
    ~SysIR()
    {
        for(auto mir:_moduleirs) delete mir;
    }
};

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cout << "Usage: " << argv[0] << " <vcfile> <vcirfile>" << endl;
        exit(1);
    }
    vcSystem::_opt_flag = true;

    vcSystem vcs("sys");
    vcs.Parse(argv[1]);
    // Setting all modules as top. Will have to borrow one or more CLI args of
    // vc2vhdl. It's mainly for vc IR to clean up unreachable modules, so
    // doesn't affect much to the simulator.
    for(auto m:vcs.Get_Modules()) vcs.Set_As_Top_Module(m.second);
    vcs.Elaborate();
    System sys(&vcs, {});

    SysIR sysir(vcs,sys);

    ofstream pfile(argv[2]);
    sysir.exportIR(pfile);
}

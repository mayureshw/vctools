using namespace std;

#include <iostream>
#include <filesystem>
#include "vc2pn.h"
#include "vcsim.h"

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

    filesystem::path vcpath(vcflnm);
    string basename = vcpath.stem();
    vcSystem vcs(basename);
    vcs.Parse(vcflnm);
    // Setting all modules as top. Will have to borrow one or more CLI args of
    // vc2vhdl. It's mainly for vc IR to clean up unreachable modules, so
    // doesn't affect much to the simulator.
    for(auto m:vcs.Get_Modules()) vcs.Set_As_Top_Module(m.second);
    vcs.Elaborate();

#   ifdef GEN_CPDOT
    vcs.Print_Reduced_Control_Paths_As_Dot_Files();
#   endif

    System sys(&vcs, daemons);
#   ifdef GEN_DPDOT
    sys.printDPDotFiles();
#   endif

#   ifdef GEN_PETRIDOT
    sys.printPNDotFile();
#   endif

#   ifdef GEN_PETRIJSON
    sys.printPNJsonFile();
#   endif

#   ifdef GEN_PETRIPNML
    sys.printPNPNMLFile();
#   endif

    sys.invoke(invoke, inpv, feeds, collects, collectopmap);
}

void VcsimIf::stop() { _sys->stop(); }

vector<DatumBase*> VcsimIf::readPipe(const string pipename, unsigned cnt)
{
    return _sys->getReader(pipename)->receive_sync(cnt);
}

void VcsimIf::feedPipe(const string pipename, const vector<DatumBase*>& feedv)
{
    _sys->getFeeder(pipename)->feed(feedv);
}

void VcsimIf::invoke()
{
    _sys->invoke("", {}, {}, {}, emptymap);
}

VcsimIf::VcsimIf(string vcflnm, const set<string>& daemons)
{
    ifstream stream;
    filesystem::path vcpath(vcflnm);
    string basename = vcpath.stem();
    stream.open(vcflnm);
    vcSystem::_opt_flag = true;
    vcSystem vcs(basename);
    vcs.Parse(vcflnm);
    for(auto m:vcs.Get_Modules()) vcs.Set_As_Top_Module(m.second);
    vcs.Elaborate();
    _sys = new System(&vcs, daemons);
}

VcsimIf::~VcsimIf() { delete _sys; }

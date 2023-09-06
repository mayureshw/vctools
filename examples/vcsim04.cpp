using namespace std;
#include <vector>
#include <iostream>
#include <thread>
#include "vcsim.h"

// Example of a low level interface to simulator, where pipe interaction is
// managed by the testbench

// Note: Do not make combined use of both interfaces i.e. vcsim function and
// VcsimIf class.

// Note that pipe reader thread must be started as a separate thread, else the
// simualtion would hang as there is no reader at the other end of the pipes.

void feeder(VcsimIf* simif)
{
    // You may send 1 record at a time in vectors of size 1 OR...
    for(int i=0; i<8; i++)
    {
        auto d = new Datum<uint8_t>(8);
        *d = i;
        simif->feedPipe("p1",{d});
    }

    // You may send vectors of size > 1 if that suits you
    auto d1 = new Datum<uint8_t>(8);
    auto d2 = new Datum<uint8_t>(8);
    *d1 = 8;
    *d2 = 9;
    simif->feedPipe("p1",{d1,d2});
}

void reader(VcsimIf* simif)
{
    // You may read chunks of size > 1 OR ...
    auto rv = simif->readPipe("p2",4);
    cout << "Read chunk";
    for(auto r:rv) cout << " " << r->str();
    cout << endl;

    // You may read vector of size 1 at a time
    for(int i=0; i<6; i++)
    {
        auto rv = simif->readPipe("p2",1);
        cout << "Read " << rv[0]->str() << endl;
    }
}

void invokesim(VcsimIf* simif)
{
    // In invoke takes no arguments as the module is expected to have only
    // daemons with no input vector and pipe interaction is handled by the
    // test bench.
    simif->invoke();
}

// Purpose of the log thread is to illustrate a scenario when, duing a running
// simulation (invoked by calling VcsimIf::invoke) a test bench wants to
// invoke a module, typically to probe some state (storage) variables.
//
// The VcsimIf::invoke API is asynchronous in the sense that it returns
// immediately and does not wait till the simulation ends. It starts the main
// simulation and it ends subject to invocation of VcsimIf::stop API. It
// doesn't invoke a specific module. It just starts the daemons specified
// during construction of the VcsimIf object.
//
// The VcsimIf::moduleInvoke is a synchronous API. It invokes a specific
// module name that it takes as an argument, it waits till the module exits
// and it returns the output vector of the module output parameters.
//
// Consider moduleInvoke as an auxiliary API meanto to interact with a running
// simulation.
//
// moduleInvoke must not invoke a daemon. It must invoke a non-daemon.
// (Daemons are identified in constructor arguments of VcsimIf.
//
// You can invoke multiple modules using multiple moduleInvoke calls
// concurrently. However, note that a given module should not be invoked until
// a previous call to it (if any) returns. If you invoke one module
// concurrently more than once, the behavior is undefined.
//
// In this illustration the log thread iteratively probes a storage variable,
// which happens to be a counter of writes by the main daemon. Do note that we
// are not initializing the storage to keep this illustration simple. So
// counter might appear to start at an arbitrary value. But the actual value
// of counter is not relevant here. We are just illustrating the moduleInvoke
// mechanism.
void log(VcsimIf* simif)
{
    int lastcnt, cnt = 0;
    for(;;)
    {
        lastcnt = cnt;
        auto opv = simif->moduleInvoke("getcounter");
        int cnt = ( ( Datum<uint8_t> *) opv[0] )->val;
        cout << "log count=" << cnt << endl;
    }
}

int main()
{
    // Specify the vC file name and daemon modules
    // No top module, no input vector
    VcsimIf simif("vcsim04.vc",{"daemon"});

    // Detach the log thread. It doesn't have a logical synchronization point
    thread tlog(log, &simif);
    tlog.detach();

    thread tfeeder(feeder, &simif);
    thread treader(reader, &simif);
    thread tinvoke(invokesim, &simif);
    tfeeder.join();
    treader.join();

    // You may optionally stop the simulation as per your stopping logic Else
    // it will keep running. In this example, we can stop the simulation after
    // the feeder and reader threads have done desired number of writes and
    // reads.
    simif.stop();
    tinvoke.join();
    return 0;
}

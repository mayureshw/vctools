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

    // You may optionally stop the simulation as per your stopping logic Else
    // it will keep running. There is no top module in this mode, hence no
    // logical end point to simulation.
    simif->stop();
}

void invokesim(VcsimIf* simif)
{
    // In invoke takes no arguments as the module is expected to have only
    // daemons with no input vector and pipe interaction is handled by the
    // test bench.
    simif->invoke();
}

int main()
{
    // Specify the vC file name and daemon modules
    // No top module, no input vector
    VcsimIf simif("vcsim04.vc",{"daemon"});
    thread tfeeder(feeder, &simif);
    thread treader(reader, &simif);
    thread tinvoke(invokesim, &simif);
    tfeeder.join();
    treader.join();
    tinvoke.join();
    return 0;
}

using namespace std;
#include <vector>
#include "vcsim.h"

int main()
{
    // Example of initializaing a vector of Datums
    vector<uint8_t> arr = { 10, 20, 30 };
    vector<DatumBase*> p1vec;
    for(auto e:arr)
    {
        auto d = new Datum<uint8_t>(8);
        *d = e;
        p1vec.push_back(d);
    }

    vector<pair<string,vector<DatumBase*>>> feeds = 
        {
        {"p1",p1vec}, // Send vector p1vec on pipe p1
                      // ... more such entries
        };
    vector<pair<string,unsigned>> collects =
        {
        {"p2",3},     // Collect 3 elements from pipe p2
                      // ... more such entries
        };

    // Pipe data specified in collects is printed on stdout. Just in case
    // we need it back, can collect it in this map
    map<string,vector<DatumBase*>> collectopmap;

    vcsim("vcsim02.vc", "syspipe", {}, feeds, collects, {}, collectopmap);

    // Getting string representation of the datum values
    cout << "Printing from test bench" << endl;
    for( auto pipeop : collectopmap )
    {
        cout << "\tRead from pipe (using ->str()) : " << pipeop.first << ":";
        for(auto d:pipeop.second) cout << " " << d->str();
        cout << endl;
    }

    // Getting raw data back from Datum vector
    for( auto pipeop : collectopmap )
    {
        cout << "\tRead from pipe (as raw value)  : " << pipeop.first << ":";
        for(auto d:pipeop.second) cout << " " << (int) ((Datum<uint8_t>*) d)->val;
        cout << endl;
    }

    return 0;
}

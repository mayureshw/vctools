using namespace std;
#include <vector>
#include "datum.h"
#include "vcsim.h"

int main()
{
    Datum<uint8_t> a(8), b(8), c(8);
    a = 10; b = 20; c = 30;
    vector<pair<string,vector<DatumBase*>>> feeds = 
        {
        {"p1",{&a,&b,&c}}
        };
    vector<pair<string,unsigned>> collects =
        {
        {"p2",3}
        };
    map<string,vector<DatumBase*>> collectopmap;
    vcsim("vcsim02.vc", "syspipe", {}, feeds, collects, {}, collectopmap);
    return 0;
}

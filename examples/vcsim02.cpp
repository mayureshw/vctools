using namespace std;
#include <vector>
#include "datum.h"
#include "vcsim.h"

int main()
{
    Datum<int> a(8), b(8), c(8);
    a = 10; b = 20; c = 30;
    vector<DatumBase*> inpv;
    vector<pair<string,vector<DatumBase*>>> feeds = 
        {
        {"p1",{&a,&b,&c}}
        };
    vector<pair<string,unsigned>> collects =
        {
        {"p2",3}
        };
    map<string,vector<DatumBase*>> collectopmap;
    vcsim("vcsim02.vc", "syspipe", inpv, feeds, collects, {}, collectopmap);
    return 0;
}
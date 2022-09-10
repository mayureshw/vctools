using namespace std;
#include <vector>
#include "datum.h"
#include "vcsim.h"

int main()
{
    Datum<int> a(8), b(8);
    a = 10; b = 20;
    vector<DatumBase*> inpv = {&a, &b};
    vcsim("vcsim01.vc", "addi", inpv);
    return 0;
}

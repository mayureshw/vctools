using namespace std;
#include <vector>
#include "datum.h"
#include "vcsim.h"

int main()
{
    vcsim("vcsim03.vc", "top", {}, {}, {}, {"daemon"});
    return 0;
}

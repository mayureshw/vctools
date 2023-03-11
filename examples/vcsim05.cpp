using namespace std;
#include <vector>
#include "datum.h"
#include "vcsim.h"

int main()
{
    vcsim("vcsim05.vc", "", {}, {}, {}, {"incr_1_daemon","incr_2_daemon"});
}

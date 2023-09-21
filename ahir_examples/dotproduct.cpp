using namespace std;
#include <vector>
#include <set>
#include <string>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include "vcsim.h"

const set<string> daemons = {
    };

VcsimIf *simif;

void Exit(int sig)
{
    fprintf(stderr, "## Break! ##\n");
    exit(0);
}

#define RETVAL(E) ((Datum<float>*)E[0])->val

int main(int argc, char* argv[])
{
    float result;
    signal(SIGINT,  Exit);
    signal(SIGTERM, Exit);

    simif = new VcsimIf( "dotproduct.vc", daemons );
    thread thrmain([simif]{simif->invoke();});
    thrmain.detach();

    simif->moduleInvoke("init");
    fprintf(stdout,"init completed.\n");

#ifdef EXPERIMENTAL
    result = RETVAL(simif->moduleInvoke("dotp_experimental"));
    fprintf(stdout,"experimental result = %f.\n", result);
#endif

#ifdef PIPELINEDUNROLLED
    fprintf(stdout,"calling dot_pipelined_unrolled.\n");
    result = RETVAL(simif->moduleInvoke("dotp_pipelined_unrolled"));
    fprintf(stdout,"pipelined-unrolled result = %f.\n", result);
#endif

#ifdef PIPELINED
    result = RETVAL(simif->moduleInvoke("dotp_pipelined"));
    fprintf(stdout,"pipelined result = %f.\n", result);
#endif

#ifdef NONPIPELINEDUNROLLED
    result = RETVAL(simif->moduleInvoke("dotp_nonpipelined_unrolled"));
    fprintf(stdout,"nonpipelined-unrolled result = %f.\n", result);
#endif

#ifdef NONPIPELINED
    result = RETVAL(simif->moduleInvoke("dotp_nonpipelined"));
    fprintf(stdout,"nonpipelined result = %f.\n", result);
#endif

    return(0);
}

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

using namespace std;
#include <thread>
#include <vector>
#include <set>
#include <string>
#include "vcsim.h"

set<string> daemons = { "p2p_ping_daemon", "p2p_pong_daemon" };
VcsimIf simif("p2p_2.vc",daemons);

#define ORDER 4
uint32_t expected_result[ORDER];

void Exit(int sig)
{
    fprintf(stderr, "## Break! ##\n");
    exit(0);
}

void Sender()
{
    int idx;
    uint32_t val[ORDER];
    for(idx = 0; idx < ORDER; idx++)
    {
        val[idx] = (2*idx)+1;
        expected_result[idx] = val[idx];
    }
    simif.write_n<uint32_t>("in_data",val,ORDER);
}


int main(int argc, char* argv[])
{
    uint32_t result[ORDER];
    signal(SIGINT,  Exit);
    signal(SIGTERM, Exit);

    thread thrmain([simif]{simif.invoke();});
    thrmain.detach();
    thread th_sender(&Sender);

    uint8_t idx;
    simif.read_n<uint32_t>("out_data",result,ORDER);

    for(idx = 0; idx < ORDER; idx++)
    {
        fprintf(stdout,"Result = %x, expected = %x.\n", result[idx],expected_result[idx]);
    }

    th_sender.join();
    simif.stop();
    return(0);
}

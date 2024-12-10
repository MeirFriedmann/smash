#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;

    SmallShell& smash = SmallShell::getInstance();
    pid_t fg_pid = smash.getFgPid();

    if (fg_pid > 0 )
    {
        if (kill(fg_pid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
        cout << "smash: process " << fg_pid << " was killed" << endl;
        smash.setFgPid(-1);
    }
}

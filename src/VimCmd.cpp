#include "../include/VimCmd.h"

#include <iostream>
using namespace std;

VimCmd::Command VimCmd::getCommandNumber(const char* command) {
    const char* cmd(commands);
    int cmdIndex = 0;

    while (*cmd) {
        const char* commandIter = command;

        while (*commandIter and (*cmd == *commandIter))
        { ++cmd; ++commandIter; }
        if ((*cmd==0 or *cmd==',') and *commandIter==0)
          return (Command)cmdIndex;
        if (*commandIter==0) return Command::CMD_UNTERMINATED;

        while (*cmd && *cmd != ',') ++cmd;
        if (*cmd) cmd++;

        ++cmdIndex;
    }

    return CMD_UNKNOWN;
}


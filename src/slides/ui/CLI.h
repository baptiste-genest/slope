#ifndef CLI_H
#define CLI_H

#include <string>

namespace slope {

// Reads the command line and sets the matching Options. Returns 0 on success.
int parseCLI(int argc,char** argv);

}

#endif // CLI_H

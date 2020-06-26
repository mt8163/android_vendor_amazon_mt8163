#ifndef DEBUG_TOOL_
#define DEBUG_TOOL_

/**
 * Shows (or displays) the usage of the tool and all
 * its available options.
 */
void show_help();

/**
 * First checks if the tool was launched from a root shell (uid=0),
 * if wasn't, it will try to find su binary and warn the user to
 * launch the tool from a root shell (using su).
 */
int check_root();

/**
 * Writes an int value to a specific file.
 */
int write_int(char *file, int value);

#endif /* DEBUG_TOOL_ */

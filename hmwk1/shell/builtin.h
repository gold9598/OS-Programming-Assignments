#ifndef _BUILTIN_H
#define _BUILTIN_H_

#include "list.h"

#define MAXHISTORY 100

extern struct List PATH;
extern struct List HISTORY;

/*
 * Adds cmd to the history list.
 * If the number of commands saved is is more than MAXHISTORY,
 * the oldest command is removed.
 */
int addToHistory(char *cmd);

/*
 * Returns a pointer to the command associated
 * with the given index from the history list.
 * Returns NULL on failure.
 */
char *getHistory(const char *index);

/*
 * Checks if cmd is a builtin command.
 * Returns 1 if it is, 0 if not.
 */
int isBuiltin(const char *cmd);

/*
 * Executes builtin command cmd.
 * args is an array of char * providing arguments to the commands.
 * args[0] should be the name of the command.
 * Returns 1 if command has completed.
 * Returns 0 if the shell's exit command has been called.
 * Returns -1 if a fatal error ahs occured.
 */
int executeBuiltin(const char *cmd, char * const args[]);

void initLists();

void cleanup();

#endif

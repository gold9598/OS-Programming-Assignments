#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "builtin.h"
#include "list.h"

struct List PATH;
struct List HISTORY;

void error(const char *err)
{
	printf("error: %s\n", err);
}

/*
 * Runs the shell's built in exit function.
 */
int runExit()
{
	return 0;
}

/*
 * Change directory to dir.
 */
int runCd(const char *dir)
{

	if (dir == NULL)
		error("Too few arguments given");

	else if (chdir(dir) < 0)
		error(strerror(errno));

	return 1;
}

/*
 * Prints all directories in the path list.
 */
void printPath()
{
	struct Node *cur = PATH.head;

	if (isEmptyList(&PATH))
		return;

	while (cur->next != NULL) {
		printf("%s:", (char *)cur->data);
		cur = cur->next;
	}

	printf("%s\n", (char *)cur->data);
}

/*
 * Adds dir to the path list.
 */
void addToPath(const char *dir)
{
	char *temp = (char *)malloc(strlen(dir) + 1);
	if (temp == NULL) {
		error("malloc failed");
		exit(EXIT_FAILURE);
	}

	strcpy(temp, dir);
	addNodeBack(&PATH, temp);
}

/*
 * Compares two strings. Returns 0 if equal.
 */
int compareString(const void *s1, const void *s2)
{
	return strcmp((char *)s1, (char *)s2);
}

/*
 * Removes all instances of dir from the path list.
 */
void removeFromPath(const char *dir)
{
	while (1) {
		struct Node *remv = findNode(&PATH, dir, &compareString);
		if (remv == NULL)
			break;

		char *temp = removeNode(&PATH, remv);
		if (temp != NULL)
			free(temp);
	}
}

/*
 * Rans the builtin path function.
 * If no action is provided, the entire path is printed.
 * If a +/- action is proved, dir is added/deleted from the path list.
 */
int runPath(const char *action, const char *dir)
{

	if (action == NULL && dir == NULL) {
		printPath();

	} else if (action == NULL || dir == NULL) {
		error("Too few arguments given");

	} else {
		if (strcmp(action, "+") == 0)
			addToPath(dir);

		else
			removeFromPath(dir);
	}

	return 1;
}

/*
 * Adds cmd to the history list.
 * If the number of commands saved is is more than MAXHISTORY,
 * the oldest command is removed.
 */
int addToHistory(char *cmd)
{

	if (numNodes(&HISTORY) >= MAXHISTORY) {
		char *temp = (char *)remvNodeFront(&HISTORY);
		free(temp);
	}

	if (addNodeBack(&HISTORY, cmd) == NULL) {
		error("malloc failed");
		exit(EXIT_FAILURE);
	}

	return 1;
}

/*
 * Tests wether testString is a number.
 * Returns 1 if yes, 0 if no.
 */
int isNumber(const char *testString)
{
	while (*testString) {
		if (!isdigit(*testString))
			return 0;

		testString++;
	}
	return 1;
}

/*
 * Returns a pointer to the command associated
 * with the given index from the history list.
 * Returns NULL on failure.
 */
char *getHistory(const char *index)
{
	if (!isNumber(index)) {
		error("invalid argument provided");
		return NULL;
	}

	int idx = atoi(index);
	struct Node *node = getNode(&HISTORY, idx - 1);
	if (node == NULL) {
		error("event not found");
		return NULL;
	}

	return (char *)node->data;
}

/*
 * Prints a list of the all the commands in the history list.
 */
int runHistory()
{

	int counter = 0;
	struct Node *curNode = HISTORY.head;

	while (curNode != NULL) {
		printf("[%d] %s\n", ++counter, (char *)curNode->data);
		curNode = curNode->next;
	}

	return 1;
}

/*
 * Checks if cmd is a builtin command.
 * Returns 1 if it is, 0 if not.
 */
int isBuiltin(const char *cmd)
{
	if (strcmp(cmd, "exit") == 0 ||
		strcmp(cmd, "cd") == 0 ||
		strcmp(cmd, "path") == 0 ||
		strcmp(cmd, "history") == 0)

		return 1;

	return 0;
}

/*
 * Executes builtin command cmd.
 * args is an array of char * providing arguments to the commands.
 * args[0] should be the name of the command.
 * Returns 1 if command has completed.
 * Returns 0 if the shell's exit command has been called.
 * Returns -1 if a fatal error ahs occured.
 */
int executeBuiltin(const char *cmd, char * const args[])
{
	if (strcmp(cmd, "exit") == 0)
		return runExit();

	else if (strcmp(cmd, "cd") == 0)
		return runCd(args[1]);

	else if (strcmp(cmd, "path") == 0)
		return runPath(args[1], args[2]);

	else if (strcmp(cmd, "history") == 0)
		return runHistory();

	return 1;
}

void initLists()
{
	initList(&HISTORY);
	initList(&PATH);
}

void cleanup()
{
	traverseList(&HISTORY, *free);
	removeAllNodes(&HISTORY);

	traverseList(&PATH, *free);
	removeAllNodes(&PATH);
}

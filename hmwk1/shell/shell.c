#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#include "list.h"
#include "builtin.h"

#define true 1
#define false 0
#define MAXARGS 128

void err(const char *err)
{
	printf("error: %s\n", err);
}

void errMalloc()
{
	err("malloc failed");
	exit(EXIT_FAILURE);
}


void cleanArgs(char *args[], const int numArgs)
{
	int x;
	for (x = 0; x < numArgs; x++)
		free(args[x]);
}

/*
 * Reallocates memory for buffer, doubling its size.
 * The new size is placed in curBufSize.
 */
char *increaseBuffer(char *buffer, int *curBufSize)
{
	*curBufSize *= 2;
	buffer = (char *)realloc(buffer, sizeof(char) * *curBufSize);
	if (buffer == NULL) {
		err("realloc failed");
		free(buffer);
		exit(EXIT_FAILURE);
	}

	return buffer;
}

/*
 * Reads input from stdin into a buffer allocated on the heap.
 * Function returns a pointer to this buffer.
 */
char *readInput()
{
	int bufSize = 64;
	int charCount = 0;
	char currChar = '\0';

	char *buffer = (char *)malloc(sizeof(char) * bufSize);
	if (buffer == NULL)
		errMalloc();

	while (currChar != '\n') {
		/* if inBuf is full, double the size */
		if (charCount == bufSize)
			buffer = increaseBuffer(buffer, &bufSize);

		currChar = getc(stdin);
		buffer[charCount] = currChar;
		charCount++;
	}

	/* remove \n and null terminate the string */
	buffer[--charCount] = '\0';

	return buffer;
}

/*
 * Parses a line into tokens.
 * A deep copy of each token is created on the heap.
 * Pointers to these strings are placed in the tokens[] array
 * of length len.
 */
int parseLine(const char *inputLine, char *tokens[], const int len)
{

	/* create a local copy of inputLine */
	char buffer[strlen(inputLine) + 1];
	strcpy(buffer, inputLine);

	/* Add each token to the end of the tokens list */
	char *tok = strtok(buffer, " \t");
	int numArgs = 0;
	do {

		tokens[numArgs] = (char *)malloc(strlen(tok) + 1);
		if (tokens[numArgs] == NULL)
			errMalloc();

		strcpy(tokens[numArgs], tok);
		numArgs++;
	} while (((tok = strtok(NULL, " ")) != NULL) && numArgs < len);


	return numArgs;
}

/*
 * Verifies a file is a file, and not a directory.
 */
int isFile(const char *file)
{
	if (opendir(file) == NULL)
		return 1;
	return 0;
}

/*
 * Search a given DIR * for file.
 * Return 1 if the file was found in the given directory,
 * returns 0 if the file was not found.
 */
int searchDirectory(DIR *dirStream, const char *file)
{
	int curErrno = errno;
	struct dirent *dir = readdir(dirStream);

	while ((dir != NULL) || (errno != curErrno)) {
		if (dir == NULL) {
			/* Error occured. Ignore and continue */
			curErrno = errno;

		} else if (strcmp(file, dir->d_name) == 0) {
			if (isFile(file))
				return 1;
		}
		dir = readdir(dirStream);
	}

	return 0;
}

/*
 * Searches in the given path list for file.
 * Returns a pointer to the path in which the file was found.
 * Returns NULL if the file was not found in any path.
 */
char *searchPath(const struct List *path, const char *file)
{
	struct Node *curNode = path->head;

	while (curNode != NULL) {
		char *curDir = curNode->data;

		/* Open each directory in the path list */
		DIR *dirStream = opendir(curDir);
		if (dirStream == NULL) {
			curNode = curNode->next;
			continue;
		}

		if (searchDirectory(dirStream, file)) {
			/* file found */
			closedir(dirStream);
			return curDir;
		}

		closedir(dirStream);
		curNode = curNode->next;
	}

	/* File not found */
	return NULL;
}

/*
 * Takes a directory where a file can be found,
 * places the full path of the file in fullPath.
 * Returns a pointer to fullPath.
 */
char *createFullPath(const char *dir, const char *file, char *fullPath)
{
	if (dir == NULL) {
		/* Just return the file */
		strcpy(fullPath, file);
		return fullPath;
	}

	strcpy(fullPath, dir);

	/* Add in the / before the file name */
	int dirLen = strlen(dir);
	if (dir[dirLen] != '/')
		strcat(fullPath, "/");

	strcat(fullPath, file);

	return fullPath;
}

/*
 * Searches for file in each directory in the path list.
 * If found returns a pointer to the complete path (allocated on the heap).
 * Returns NULL if the file cannot be found in the path.
 */
char *getFullPath(const struct List *path, const char *file)
{
	char *fullPath;

	/* If file contains any / characters, it should be
	   interpreted as complete path. */
	int i;
	for (i = 0; i < strlen(file); i++) {
		if (file[i] == '/') {
			fullPath = (char *)malloc(strlen(file) + 1);
			if (fullPath == NULL)
				errMalloc();

			strcpy(fullPath, file);
			return fullPath;
		}
	}

	char *dir = searchPath(path, file);
	if (dir == NULL) {
		err("no such file or directory");
		return NULL;
	}

	/* malloc an extra space in case an extra '/' is needed */
	fullPath = (char *)malloc(strlen(dir) + strlen(file) + 2);
	if (fullPath == NULL)
		errMalloc();

	createFullPath(dir, file, fullPath);
	return fullPath;
}

/*
 * Attempts to execute command.
 * args is an array of NULL-terminated strings passed to the command.
 * args[0] should point to the name of the command.
 * args must be terminated by a NULL pointer.
 * Returns 1 if command has completed.
 * Returns 0 if shell should be closed.
 * Returns -1 if fatal error has occured.
 */
int commandHandler(const char *command, char * const args[])
{

	if (isBuiltin(command)) {
		return executeBuiltin(command, args);

	} else {
		char *fullPath = getFullPath(&PATH, command);
		if (fullPath == NULL) {
			/* If file not found in path, return */
			free(fullPath);
			return 1;
		}


		/* fork and execute command */
		pid_t pid = fork();
		if (pid == 0) {
			/* child */
			execv(fullPath, args);

			free(fullPath);
			err(strerror(errno));
			return -1;
		}

		else if (pid < 0) {
			err(strerror(errno));
			return -1;
		}

		else {
			/* parent */
			free(fullPath);
			wait(NULL);
			return 1;
		}
	}
}

int main(const int argc, const char **argv)
{
	int stillRunning = true;
	char *inputLine;
	char *command;
	int numArgs;

	initLists();

	while (stillRunning) {
		char *args[MAXARGS + 1] = {0};

		printf("$ ");

		inputLine = readInput();
		if (inputLine == NULL) {
			err("Error reading input");
			free(inputLine);
			continue;
		}
		/* If no input was given, display prompt */
		if (strlen(inputLine) < 1) {
			free(inputLine);
			continue;
		}

		if (inputLine[0] == '!') {
			/* Replace inputLine with the nth command */
			char *temp = getHistory(inputLine + 1);
			free(inputLine);

			if (temp == NULL)
				continue;

			inputLine = (char *)malloc(strlen(temp) + 1);
			if (inputLine == NULL)
				errMalloc();
			strcpy(inputLine, temp);

		}

		addToHistory(inputLine);

		numArgs = parseLine(inputLine, args, MAXARGS);
		command = args[0];

		if (commandHandler(command, args) <= 0) {
			/* Exit Shell */
			stillRunning = false;
			cleanArgs(args, numArgs);
			break;
		}

		cleanArgs(args, numArgs);

	}

	cleanup();
	return 0;
}

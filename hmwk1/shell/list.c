#include <stdio.h>
#include <stdlib.h>

#include "list.h"

/*
 * Return a pointer to the last node in the list.
 * Returns NULL if the list is empty.
 */
struct Node *getLastNode(struct List *list)
{
	if (isEmptyList(list))
		return NULL;

	struct Node *lastNode = list->head;

	while (lastNode->next != NULL)
		lastNode = lastNode->next;

	return lastNode;
}

/*
 * Add a new node to the end of the list.
 * Returns a pointer to the new node on success.
 * Returns NULL on failure.
 */
struct Node *addNodeBack(struct List *list, void *data)
{

	/* create the new node */
	struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));

	if (newNode == NULL)
		return NULL;

	newNode->data = data;
	newNode->next = NULL;
	newNode->prev = NULL;

	/* Check if this is first node */
	if (list->head == NULL) {
		list->head = newNode;
		return newNode;
	}

	/* Add newNode to the end */
	struct Node *lastNode = getLastNode(list);
	lastNode->next = newNode;
	newNode->prev = lastNode;

	return newNode;
}

/*
 * Removes a node from the front of the list.
 * Returns a pointer to the removed node's data.
 * Returns NULL if the list is empty.
 */
void *remvNodeFront(struct List *list)
{
	if (isEmptyList(list))
		return NULL;

	struct Node *temp = list->head;
	list->head = temp->next;
	if (temp->next != NULL)
		list->head->prev = NULL;

	void *data = temp->data;
	free(temp);

	return data;
}

/*
 * Removes all nodes in the list and deallocates the memory.
 */
void removeAllNodes(struct List *list)
{
	void *temp = (void *)list->head;

	while (temp != NULL)
		temp = remvNodeFront(list);
}

/*
 * Returns the number of nodes in the list.
 * Returns 0 if the list is empty.
 */
int numNodes(struct List *list)
{
	if (isEmptyList(list))
		return 0;

	int counter = 1;
	struct Node *temp = list->head;
	while (temp->next != NULL) {
		counter++;
		temp = temp->next;
	}

	return counter;
}

/*
 * Traverse the list, calling function f() on each node's data
 */
void traverseList(struct List *list, void (*f)(void *))
{
	struct Node *cur = list->head;
	while (cur != NULL) {
		f(cur->data);
		cur = cur->next;
	}
}

/*
 * Searches list for dataWant using the compare() function.
 * Compare() must return 0 if a the two elements are equal.
 * Will return first occurence of node containing dataWant.
 * If dataWant is not found, will return NULL.
 */
struct Node *findNode(struct List *list, const void *dataWant,
		int (*compare)(const void *, const void *)) {

	struct Node *cur = list->head;

	while (cur != NULL) {
		int match = compare(dataWant, cur->data);
		if (match == 0)
			return cur;

		cur = cur->next;
	}

	/* No match found */
	return NULL;
}

/*
 * Remove node from the list.
 * Returns the removed node's data.
 */
void *removeNode(struct List *list, struct Node *node)
{

	struct Node *temp = node;
	struct Node *prevNode = node->prev;
	struct Node *nextNode = node->next;

	if (prevNode == NULL) {
		list->head = nextNode;
		if (nextNode != NULL)
			nextNode->prev = NULL;

	} else if (nextNode == NULL) {
		prevNode->next = NULL;

	} else {
		prevNode->next = nextNode;
		nextNode->prev = prevNode;
	}

	void *data = temp->data;
	free(temp);
	return data;
}

/*
 * Returns a pointer to the node at the given index.
 * Returns NULL if a node at that index does not exist
 */
struct Node *getNode(struct List *list, int index)
{
	int counter = 0;
	struct Node *cur = list->head;

	while (cur != NULL) {
		if (counter == index)
			return cur;
		cur = cur->next;
		counter++;
	}

	return NULL;
}

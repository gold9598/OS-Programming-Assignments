#ifndef _LIST_H_
#define _LIST_H_

struct Node {
	void *data;
	struct Node *next;
	struct Node *prev;
};

struct List {
	struct Node *head;
};

static inline void initList(struct List *list)
{
	list->head = NULL;
}

static inline int isEmptyList(struct List *list)
{
	return (list->head == NULL);
}

/*
 * Return a pointer to the last node in the list.
 * Returns NULL if the list is empty.
 */
struct Node *getLastNode(struct List *list);

/*
 * Add a new node to the end of the list.
 * Returns a pointer to the new node on success.
 * Returns NULL on failure.
 */
struct Node *addNodeBack(struct List *list, void *data);

/*
 * Removes a node from the front of the list.
 * Returns a pointer to the removed node's data.
 * Returns NULL if the list is empty.
 */
void *remvNodeFront(struct List *list);

/*
 * Removes all nodes in the list and deallocates the memory.
 */
void removeAllNodes(struct List *list);

/*
 * Returns the number of nodes in the list.
 * Returns 0 is the list is empty.
 */
int numNodes(struct List *list);

/*
 * Traverse the list, calling function f() on each node's data
 */
void traverseList(struct List *list, void (*f)(void *));

/*
 * Searches list for dataWant using the compare() function.
 * Compare() must return 0 if a the two elements are equal.
 * Will return first occurence of node containing dataWant.
 * If dataWant is not found, will return NULL.
 */
struct Node *findNode(struct List *list, const void *dataWant,
		int (*compare)(const void *, const void *));

/*
 * Remove a node from the list.
 * Returns the removed node's data.
 */
void *removeNode(struct List *list, struct Node *node);

/*
 * Returns a pointer to the node at the given index.
 * Returns NULL if a node at that index does not exist.
 */
struct Node *getNode(struct List *list, int index);

#endif

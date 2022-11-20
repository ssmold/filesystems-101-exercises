#include <solution.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


typedef struct node {
    int isLeaf;
    int numKeys;
    int* keys; // TODO: limit by L elems
    struct node **kids; // TODO: limit by L + 1 elems;
} *bNode;

typedef struct btree
{
    unsigned int L;
    struct node* root;
} *bTree;

//bNode node_alloc(unsigned int L) {
//    bNode node;
//
//    node = (bNode)calloc(sizeof(*node), 1);
//    node->isLeaf = 0;
//    node->numKeys =
//
//}



struct btree* btree_alloc(unsigned int L)
{
    bTree tree;
    bNode root;

    root = (bNode)calloc(sizeof(*root), 1);
    tree = (bTree)calloc(sizeof(*tree), 1);

    root->isLeaf = 1;
    root->numKeys = 0;
    root->keys = (int*)malloc(2 * L * sizeof(int));
    root->kids = NULL;

    tree->L = L;
    tree->root = root;

	return tree;
}

void node_free(bNode n)
{
    int i;

    if(!n->isLeaf) {
        for(i = 0; i < n->numKeys + 1; i++) {
            node_free(n->kids[i]);
        }
        free(n->kids);
    }
    free(n);
}

void btree_free(bTree t)
{
	if (t == NULL) {
        return;
    }

    node_free(t->root);
    free(t);
}

static int search_key(int n, const int *arr, int key)
{
    int left;
    int right;
    int mid;

    left = -1;
    right = n;

    while(left + 1 < right) {
        mid = (left + right) / 2;

        if (arr[mid] == key) {
            return mid;
        } else if (arr[mid] < key) {
            left = mid;
        } else {
            right = mid;
        }
    }

    return right;
}

int node_search(bNode node, int key)
{
    int pos;

    if (node->numKeys == 0) {
        return 0;
    }

    pos = search_key(node->numKeys, node->keys, key);

    if(pos < node->numKeys && node->keys[pos] == key) {
        return 1;
    } else {
        return(!node->isLeaf && node_search(node->kids[pos], key));
    }
}

int btree_search(bTree tree, int key) {
    if (tree == NULL) {
        return 0;
    }

    return node_search(tree->root, key);
}

void btree_insert(struct btree *t, int x)
{
	(void) t;
	(void) x;
}

void btree_delete(struct btree *t, int x)
{
	(void) t;
	(void) x;
}

bool btree_contains(struct btree *t, int x)
{
	(void) t;
	(void) x;

	return false;
}

struct btree_iter
{
};

struct btree_iter* btree_iter_start(struct btree *t)
{
	(void) t;

	return NULL;
}

void btree_iter_end(struct btree_iter *i)
{
	(void) i;
}

bool btree_iter_next(struct btree_iter *i, int *x)
{
	(void) i;
	(void) x;

	return false;
}

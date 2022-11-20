#include <solution.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

static int MAX_KEYS;
typedef struct node {
    int isLeaf;
    int numKeys;
    int *keys; // TODO: limit by L elems
    struct node **kids; // TODO: limit by L + 1 elems;
} *bNode;

typedef struct btree {
    struct node *root;
} *bTree;


struct btree *btree_alloc(unsigned int L) {
    bTree tree;
    bNode root;

    root = (bNode) calloc(sizeof(*root), 1);
    tree = (bTree) calloc(sizeof(*tree), 1);

    MAX_KEYS = 2 * L;
    root->isLeaf = 1;
    root->numKeys = 0;
    root->keys = (int *) malloc(2 * L * sizeof(int));
    root->kids = NULL;

    tree->root = root;

    return tree;
}

void node_free(bNode n) {
    int i;

    if (!n->isLeaf) {
        for (i = 0; i < n->numKeys + 1; i++) {
            node_free(n->kids[i]);
        }
        free(n->kids);
    }
    free(n->keys);
    free(n);
}

void btree_free(bTree t) {
    if (t == NULL) {
        return;
    }

    node_free(t->root);
    free(t);
}

static int search_key(int n, const int *arr, int key) {
    int left;
    int right;
    int mid;

    left = -1;
    right = n;

    while (left + 1 < right) {
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

int node_search(bNode node, int key) {
    int pos;

    if (node->numKeys == 0) {
        return 0;
    }

    pos = search_key(node->numKeys, node->keys, key);

    if (pos < node->numKeys && node->keys[pos] == key) {
        return 1;
    } else {
        return (!node->isLeaf && node_search(node->kids[pos], key));
    }
}


static bNode btree_split(bNode b, int key, int *median) {
    int pos;
    int mid;
    bNode b2;

    pos = search_key(b->numKeys, b->keys, key);

    if (pos < b->numKeys && b->keys[pos] == key) {
        return 0;
    }

    if (b->isLeaf) {

        memmove(&b->keys[pos + 1], &b->keys[pos], sizeof(*(b->keys)) * (b->numKeys - pos));
        b->keys[pos] = key;
        b->numKeys++;

    } else {

        b2 = btree_split(b->kids[pos], key, &mid);

        if (b2) {

            memmove(&b->keys[pos + 1], &b->keys[pos], sizeof(*(b->keys)) * (b->numKeys - pos));
            memmove(&b->kids[pos + 2], &b->kids[pos + 1], sizeof(*(b->keys)) * (b->numKeys - pos));

            b->keys[pos] = mid;
            b->kids[pos + 1] = b2;
            b->numKeys++;
        }
    }

    if (b->numKeys >= MAX_KEYS) {
        mid = b->numKeys / 2;

        *median = b->keys[mid];
        b2 = malloc(sizeof(*b2));

        b2->numKeys = b->numKeys - mid - 1;
        b2->isLeaf = b->isLeaf;

        memmove(b2->keys, &b->keys[mid + 1], sizeof(*(b->keys)) * b2->numKeys);
        if (!b->isLeaf) {
            memmove(b2->kids, &b->kids[mid + 1], sizeof(*(b->kids)) * (b2->numKeys + 1));
        }

        b->numKeys = mid;

        return b2;
    } else {
        return 0;
    }
}


void btree_insert(bTree t, int key) {

    bNode b1;
    bNode b2;
    int median;

    b2 = btree_split(t->root, key, &median);

    if (b2) {

        b1 = malloc(sizeof(*b1));
        assert(b1);

        /* copy root to b1 */
        memmove(b1, t->root, sizeof(*t->root));

        /* make root point to b1 and b2 */
        t->root->numKeys = 1;
        t->root->isLeaf = 0;
        t->root->keys[0] = median;
        t->root->kids[0] = b1;
        t->root->kids[1] = b2;
    }

}

void btree_delete(struct btree *t, int x) {
    (void) t;
    (void) x;
}

bool btree_contains(struct btree *tree, int key) {
    if (tree == NULL) {
        return 0;
    }

    return node_search(tree->root, key);
}

struct btree_iter {
};

struct btree_iter *btree_iter_start(struct btree *t) {
    (void) t;

    return NULL;
}

void btree_iter_end(struct btree_iter *i) {
    (void) i;
}

bool btree_iter_next(struct btree_iter *i, int *x) {
    (void) i;
    (void) x;

    return false;
}

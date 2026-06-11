/*
 *  binary_tree.h
 *  Fixed API for the concurrent binary search tree.
 *
 *  You implement these functions in binary_tree.c. Multiple threads
 *  will call them concurrently on the same tree; your internal
 *  synchronization is what keeps the tree consistent.
 *
 *  You MAY add fields to TreeNode (e.g., an omp_lock_t per node).
 *  If you do, submit the modified binary_tree.h with your solution.
 *  You may NOT change any function signature.
 */

#ifndef BINARY_TREE_H
#define BINARY_TREE_H

#include <stdbool.h>

typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
    /* Add fields here if your synchronization strategy needs them. */
} TreeNode;

/* Allocate and return a new node with the given data. */
TreeNode *createNode(int data);

/* Insert `data` into the tree. Returns the (possibly updated) root. */
TreeNode *insertNode(TreeNode *root, int data);

/* Delete `data` from the tree if present. Returns the (possibly
 * updated) root. */
TreeNode *deleteNode(TreeNode *root, int data);

/* Returns true iff `data` is currently in the tree. */
bool searchNode(TreeNode *root, int data);

/* Returns the node holding the smallest value in the tree, or NULL
 * if the tree is empty. The returned pointer is for read-only
 * inspection -- another thread may delete the node after you receive
 * it. */
TreeNode *findMin(TreeNode *root);

/* Traversals: print every value to stdout, separated by single
 * spaces, in the corresponding order. The harness uses
 * inorderTraversal to verify correctness, so its output must be the
 * sorted list of values in the tree. */
void inorderTraversal(TreeNode *root);


/* Free every node in the tree (and destroy any locks you added).
 * Does not print anything. */
void freeTree(TreeNode *root);

#endif /* BINARY_TREE_H */

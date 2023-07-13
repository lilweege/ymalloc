#include "rbtree.h"
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void RB_DumpTreeGraphvizImpl(FILE* fp, BlockNode* root) {
    if (root == NULL) return;
    BlockNode* left = RB_NODE_LEFT(root);
    BlockNode* right = RB_NODE_RIGHT(root);
    if (left != NULL) {
        fprintf(fp, "  \"%p (%ld)\" -> \"%p (%ld)\"\n",
                root, RB_NODE_KEY(root),
                left, RB_NODE_KEY(left));
        RB_DumpTreeGraphvizImpl(fp, left);
    }
    if (right != NULL) {
        fprintf(fp, "  \"%p (%ld)\" -> \"%p (%ld)\"\n",
                root, RB_NODE_KEY(root),
                right, RB_NODE_KEY(right));
        RB_DumpTreeGraphvizImpl(fp, right);
    }
}

static void RB_DumpTreeGraphviz(FILE* fp, BlockNode* root) {
    fprintf(fp, "digraph{\n");
    RB_DumpTreeGraphvizImpl(fp, root);
    fprintf(fp, "}\n");
    fflush(fp);
}

static int64_t RB_Compare(BlockNode* a, BlockNode* b) {
    int64_t cmp = RB_NODE_KEY(a) - RB_NODE_KEY(b);
    if (cmp != 0) return cmp;
    return ((ptrdiff_t)a) - ((ptrdiff_t)b);
}

static bool RB_IsRed(BlockNode* x) {
    if (x == NULL) return false;
    return RB_NODE_COLOR(x) == RB_RED;
}

static bool RB_IsEmpty(BlockNode* x) {
    return x == NULL;
}

// make a left-leaning link lean to the right
static BlockNode* RB_RotateRight(BlockNode* h) {
    assert(h != NULL && RB_IsRed(RB_NODE_LEFT(h)));
    // assert (h != NULL) && isRed(h->link[0]) &&  !isRed(h->link[1]);  // for insertion only
    BlockNode* x = RB_NODE_LEFT(h);
    RB_NODE_SET_LEFT(h, RB_NODE_RIGHT(x));
    RB_NODE_SET_RIGHT(x, h);
    RB_NODE_SET_COLOR(x, RB_NODE_COLOR(h));
    RB_NODE_SET_COLOR_RED(h);
    return x;
}

// make a right-leaning link lean to the left
static BlockNode* RB_RotateLeft(BlockNode* h) {
    assert(h != NULL && RB_IsRed(RB_NODE_RIGHT(h)));
    // assert (h != NULL) && isRed(h->link[1]) && !isRed(h->link[0]);  // for insertion only
    BlockNode* x = RB_NODE_RIGHT(h);
    RB_NODE_SET_RIGHT(h, RB_NODE_LEFT(x));
    RB_NODE_SET_LEFT(x, h);
    RB_NODE_SET_COLOR(x, RB_NODE_COLOR(h));
    RB_NODE_SET_COLOR_RED(h);
    return x;
}

// flip the colors of a node and its two children
static void RB_FlipColors(BlockNode* h) {
    // h must have opposite color of its two children
    assert((h != NULL) && (RB_NODE_LEFT(h) != NULL) && (RB_NODE_RIGHT(h) != NULL));
    assert((!RB_IsRed(h) &&  RB_IsRed(RB_NODE_LEFT(h)) &&  RB_IsRed(RB_NODE_RIGHT(h)))
        || ( RB_IsRed(h) && !RB_IsRed(RB_NODE_LEFT(h)) && !RB_IsRed(RB_NODE_RIGHT(h))));

    RB_NODE_FLIP_COLOR(h);
    RB_NODE_FLIP_COLOR(RB_NODE_LEFT(h));
    RB_NODE_FLIP_COLOR(RB_NODE_RIGHT(h));
}

// Assuming that h is red and both h.left and h.left.left
// are black, make h.left or one of its children red.
static BlockNode* RB_MoveRedLeft(BlockNode* h) {
    assert(h != NULL);
    assert(RB_IsRed(h) && !RB_IsRed(RB_NODE_LEFT(h)) && !RB_IsRed(RB_NODE_LEFT(RB_NODE_LEFT(h))));

    RB_FlipColors(h);
    if (RB_IsRed(RB_NODE_LEFT(RB_NODE_RIGHT(h)))) {
        RB_NODE_SET_RIGHT(h, RB_RotateRight(RB_NODE_RIGHT(h)));
        h = RB_RotateLeft(h);
        RB_FlipColors(h);
    }
    return h;
}

// Assuming that h is red and both h.right and h.right.left
// are black, make h.right or one of its children red.
static BlockNode* RB_MoveRedRight(BlockNode* h) {
    assert(h != NULL);
    assert(RB_IsRed(h) && !RB_IsRed(RB_NODE_RIGHT(h)) && !RB_IsRed(RB_NODE_LEFT(RB_NODE_RIGHT(h))));

    RB_FlipColors(h);
    if (RB_IsRed(RB_NODE_LEFT(RB_NODE_LEFT(h)))) {
        h = RB_RotateRight(h);
        RB_FlipColors(h);
    }
    return h;
}

// restore red-black tree invariant
static BlockNode* RB_Balance(BlockNode* h) {
    assert(h != NULL);

    if (RB_IsRed(RB_NODE_RIGHT(h)) && !RB_IsRed(RB_NODE_LEFT(h)))               h = RB_RotateLeft(h);
    if (RB_IsRed(RB_NODE_LEFT(h))  &&  RB_IsRed(RB_NODE_LEFT(RB_NODE_LEFT(h)))) h = RB_RotateRight(h);
    if (RB_IsRed(RB_NODE_LEFT(h))  &&  RB_IsRed(RB_NODE_RIGHT(h)))              RB_FlipColors(h);

    return h;
}

// the smallest key in the subtree rooted at x greater than or equal to the given key
static BlockNode* RB_CeilingImpl(BlockNode* x, RB_Key key) {
    if (x == NULL) return NULL;
    RB_Key cmp = key - RB_NODE_KEY(x);
    if (cmp == 0) return x;
    if (cmp > 0)  return RB_CeilingImpl(RB_NODE_RIGHT(x), key);
    BlockNode* t = RB_CeilingImpl(RB_NODE_LEFT(x), key);
    if (t != NULL) return t;
    else           return x;
}

BlockNode* RB_Ceiling(BlockNode* root, size_t size) {
    assert(!RB_IsEmpty(root));
    RB_Key key = size >> 1;
    return RB_CeilingImpl(root, key);
}

static BlockNode* RB_DeleteMinImpl(BlockNode* h) {
    assert(h != NULL);
    if (RB_NODE_LEFT(h) == NULL)
        return NULL;

    if (!RB_IsRed(RB_NODE_LEFT(h)) && !RB_IsRed(RB_NODE_LEFT(RB_NODE_LEFT(h))))
        h = RB_MoveRedLeft(h);

    RB_NODE_SET_LEFT(h, RB_DeleteMinImpl(RB_NODE_LEFT(h)));
    return RB_Balance(h);
}

static void RB_TransplantNode(BlockNode** root, BlockNode* targetParent, BlockNode* target, BlockNode* newNode, bool targetIsRoot, bool targetIsLeft) {
    assert(targetIsRoot || targetParent != NULL);
    assert(newNode != NULL);
    BlockNode* left = RB_NODE_LEFT(target);
    BlockNode* right = RB_NODE_RIGHT(target);
    NodeColor color = RB_NODE_COLOR(target);

    if (targetIsRoot)
        *root = newNode;
    else if (targetIsLeft)
        RB_NODE_SET_LEFT(targetParent, newNode);
    else
        RB_NODE_SET_RIGHT(targetParent, newNode);

    RB_NODE_SET_LEFT(newNode, left);
    RB_NODE_SET_RIGHT(newNode, right);
    RB_NODE_SET_COLOR(newNode, color);

    RB_NODE_SET_LEFT(target, NULL);
    RB_NODE_SET_RIGHT(target, NULL);
}

// delete the key-value pair with the given key rooted at h
static BlockNode* RB_DeleteImpl(BlockNode** root, BlockNode* parent, BlockNode* h, BlockNode* toDelete) {
    if (RB_Compare(toDelete, h) < 0)  {
        if (!RB_IsRed(RB_NODE_LEFT(h)) && !RB_IsRed(RB_NODE_LEFT(RB_NODE_LEFT(h))))
            h = RB_MoveRedLeft(h);
        RB_NODE_SET_LEFT(h, RB_DeleteImpl(root, h, RB_NODE_LEFT(h), toDelete));
    }
    else {
        if (RB_IsRed(RB_NODE_LEFT(h)))
            h = RB_RotateRight(h);
        if (RB_Compare(toDelete, h) == 0 && (RB_NODE_RIGHT(h) == NULL))
            return NULL;
        if (!RB_IsRed(RB_NODE_RIGHT(h)) && !RB_IsRed(RB_NODE_LEFT(RB_NODE_RIGHT(h))))
            h = RB_MoveRedRight(h);
        if (RB_Compare(toDelete, h) == 0) {
            // Current root of subtree h has same toDelete key, delete this node (h)
            BlockNode* x = RB_NODE_RIGHT(h);
            while (RB_NODE_LEFT(x) != NULL) {
                x = RB_NODE_LEFT(x);
            }

            assert(RB_NODE_LEFT(x) == NULL);
            assert(RB_NODE_RIGHT(x) == NULL);
            RB_NODE_SET_RIGHT(h, RB_DeleteMinImpl(RB_NODE_RIGHT(h)));

            bool targetIsRoot = parent == NULL;
            bool targetIsLeft = !targetIsRoot && (RB_NODE_LEFT(parent) != NULL && RB_Compare(RB_NODE_LEFT(parent), h) == 0);
            RB_TransplantNode(root, parent, h, x, targetIsRoot, targetIsLeft);
            h = targetIsRoot ? *root :
                targetIsLeft ? RB_NODE_LEFT(parent) : RB_NODE_RIGHT(parent);
        }
        else RB_NODE_SET_RIGHT(h, RB_DeleteImpl(root, h, RB_NODE_RIGHT(h), toDelete));
    }
    return RB_Balance(h);
}

void RB_Delete(BlockNode** root, BlockNode* toDelete) {
    bool found = false;
    BlockNode* x = *root;
    while (x != NULL) {
        int64_t cmp = RB_Compare(toDelete, x);
        if (cmp == 0) {
            found = true;
            break;
        }
        x = cmp < 0 ? RB_NODE_LEFT(x) : RB_NODE_RIGHT(x);
    }
    if (!found) return;

    // if both children of root are black, set root to red
    if (!RB_IsRed(RB_NODE_LEFT(*root)) && !RB_IsRed(RB_NODE_RIGHT(*root)))
        RB_NODE_SET_COLOR_RED(*root);

    *root = RB_DeleteImpl(root, NULL, *root, toDelete);
    if (!RB_IsEmpty(*root)) RB_NODE_SET_COLOR_BLACK(*root);
}

// insert the key-value pair in the subtree rooted at h
static BlockNode* RB_PutImpl(BlockNode* h, BlockNode* toInsert) {
    if (h == NULL) return toInsert;

    assert(RB_NODE_LEFT(h) != h);
    assert(RB_NODE_RIGHT(h) != h);

    int64_t cmp = RB_Compare(toInsert, h);
    if      (cmp < 0) RB_NODE_SET_LEFT(h, RB_PutImpl(RB_NODE_LEFT(h),  toInsert));
    else if (cmp > 0) RB_NODE_SET_RIGHT(h, RB_PutImpl(RB_NODE_RIGHT(h), toInsert));
    else assert(false); // Inserting same node twice, should be impossible

    // fix-up any right-leaning links
    return RB_Balance(h);
}

void RB_Put(BlockNode** root, BlockNode* toInsert) {
    assert(toInsert != NULL);
    RB_NODE_SET_COLOR_RED(toInsert);

    *root = RB_PutImpl(*root, toInsert);
    RB_NODE_SET_COLOR_BLACK(*root);
}


// is the tree rooted at x a BST with all keys strictly between min and max
static bool RB_IsBSTImpl(BlockNode* x, RB_Key min, RB_Key max) {
    if (x == NULL) return true;
    RB_Key key = RB_NODE_KEY(x);
    if (key < min || key > max) return false;
    return RB_IsBSTImpl(RB_NODE_LEFT(x),  min, RB_NODE_KEY(x)) &&
           RB_IsBSTImpl(RB_NODE_RIGHT(x), RB_NODE_KEY(x), max);
}

// does this binary tree satisfy symmetric order?
// Note: this test also ensures that data structure is a binary tree since order is strict
static bool RB_IsBST(BlockNode* root) {
    return RB_IsBSTImpl(root, INT64_MIN, INT64_MAX);
}

// Does the tree have no red right links, and at most one (left)
// red links in a row on any path?
static bool RB_Is23Impl(BlockNode* root, BlockNode* x) {
    if (x == NULL) return true;
    if (RB_IsRed(RB_NODE_RIGHT(x))) return false;
    if (x != root && RB_IsRed(x) && RB_IsRed(RB_NODE_LEFT(x)))
        return false;
    return RB_Is23Impl(root, RB_NODE_LEFT(x)) && RB_Is23Impl(root, RB_NODE_RIGHT(x));
}

static bool RB_Is23(BlockNode* root) { return RB_Is23Impl(root, root); }
// do all paths from root to leaf have same number of black edges?
// does every path from the root to a leaf have the given number of black links?
static bool RB_IsBalancedImpl(BlockNode* x, int black) {
    if (x == NULL) return black == 0;
    if (!RB_IsRed(x)) black--;
    return RB_IsBalancedImpl(RB_NODE_LEFT(x), black) && RB_IsBalancedImpl(RB_NODE_RIGHT(x), black);
}

static bool RB_IsBalanced(BlockNode* root) {
    int black = 0;     // number of black links on path from root to min
    BlockNode* x = root;
    while (x != NULL) {
        if (!RB_IsRed(x)) black++;
        x = RB_NODE_LEFT(x);
    }
    return RB_IsBalancedImpl(root, black);
}

void RB_AssertInvariants(BlockNode* root) {
    assert(RB_IsBST(root));
    assert(RB_Is23(root));
    assert(RB_IsBalanced(root));
}


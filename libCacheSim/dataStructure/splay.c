/*
 This splay sTree is modified by Juncheng Yang for use in LRUAnalyzer.
 Juncheng Yang <peter.waynechina@gmail.com>
 May 2016
 */
/*
 An implementation of top-down splaying
 D. Sleator <sleator@cs.cmu.edu>
 March 1992
 
 "Splay sTrees", or "self-adjusting search sTrees" are a simple and
 efficient data structure for storing an ordered set.  The data
 structure consists of a binary sTree, without parent pointers, and no
 additional fields.  It allows searching, insertion, deletion,
 deletemin, deletemax, splitting, joining, and many other operations,
 all with amortized logarithmic performance.  Since the sTrees adapt to
 the sequence of requests, their performance on real access patterns is
 typically even better.  Splay sTrees are described in a number of texts
 and papers [1,2,3,4,5].
 
 The code here is adapted from simple top-down splay, at the bottom of
 page 669 of [3].  It can be obtained via anonymous ftp from
 spade.pc.cs.cmu.edu in directory /usr/sleator/public.
 
 The chief modification here is that the splay operation works even if the
 key being splayed is not in the sTree, and even if the sTree root of the
 sTree is NULL.  So the line:
 
 t = splay(i, t);
 
 causes it to search for key with key i in the sTree rooted at t.  If it's
 there, it is splayed to the root.  If it isn't there, then the node put
 at the root is the last one before NULL that would have been reached in a
 normal binary search for i.  (It's a neighbor of i in the sTree.)  This
 allows many other operations to be easily implemented, as shown below.
 
 [1] "Fundamentals of data structures in C", Horowitz, Sahni,
 and Anderson-Freed, Computer Science Press, pp 542-547.
 [2] "Data Structures and Their Algorithms", Lewis and Denenberg,
 Harper Collins, 1991, pp 243-251.
 [3] "Self-adjusting Binary Search sTrees" Sleator and Tarjan,
 JACM Volume 32, No 3, July 1985, pp 652-686.
 [4] "Data Structure and Algorithm Analysis", Mark Weiss,
 Benjamin Cummins, 1992, pp 119-130.
 [5] "Data Structures, Algorithms, and Performance", Derick Wood,
 Addison-Wesley, 1993, pp 367-375.
 */



#ifdef __cplusplus
extern "C"
{
#endif

#include "splay.h"


static sTree * sedgewickized_splay (int i, sTree * t);

sTree * splay (key_type i, sTree * t) {
    /* Simple top down splay, not requiring i to be in the sTree t.  */
    /* What it does is described above.                             */
    sTree N, *l, *r, *y;
    if (t == NULL) return t;
    N.left = N.right = NULL;
    l = r = &N;
    long l_size=0, r_size=0;
    
    for (;;) {
        if (key_cmp(i, t->key) < 0){
            if (t->left == NULL) break;
            if (key_cmp(i, t->left->key) < 0) {
                y = t->left;                           /* rotate right */
                t->left = y->right;
                y->right = t;
                t->value = node_value(t->left) + node_value(t->right) + 1;
                t = y;
                if (t->left == NULL) break;
            }
            r->left = t;                               /* link right */
            r = t;
            t = t->left;
            r_size += 1+node_value(r->right);
        } else if (key_cmp(i, t->key) > 0) {
            if (t->right == NULL) break;
            if (key_cmp(i, t->right->key) > 0) {
                y = t->right;                          /* rotate left */
                t->right = y->left;
                y->left = t;
                t->value = node_value(t->left) + node_value(t->right) + 1;
                t = y;
                if (t->right == NULL) break;
            }
            l->right = t;                              /* link left */
            l = t;
            t = t->right;
            l_size += 1+node_value(l->left);
        } else {
            break;
        }
    }
    
    // TODO: there should be a better way to do this!!!!!!!!!
    
    l_size += node_value(t->left);  /* Now l_size and r_size are the sizes of */
    r_size += node_value(t->right); /* the left and right sTrees we just built.*/
    t->value = l_size + r_size + 1;
    
    l->right = r->left = NULL;
    
    /* The following two loops correct the size fields of the right path  */
    /* from the left child of the root and the right path from the left   */
    /* child of the root.                                                 */
    for (y = N.right; y != NULL; y = y->right) {
        y->value = l_size;
        l_size -= 1+node_value(y->left);
    }
    for (y = N.left; y != NULL; y = y->left) {
        y->value = r_size;
        r_size -= 1+node_value(y->right);
    }

    l->right = t->left;                                /* assemble */
    r->left = t->right;
    t->left = N.right;
    t->right = N.left;
    return t;
}

/* Here is how sedgewick would have written this.                    */
/* It does the same thing.                                           */
static sTree * sedgewickized_splay (int i, sTree * t) {
    sTree N, *l, *r, *y;
    if (t == NULL) return t;
    N.left = N.right = NULL;
    l = r = &N;
    
    for (;;) {
        if (i < t->key) {
            if (t->left != NULL && i < t->left->key) {
                y = t->left; t->left = y->right; y->right = t; t = y;
            }
            if (t->left == NULL) break;
            r->left = t; r = t; t = t->left;
        } else if (i > t->key) {
            if (t->right != NULL && i > t->right->key) {
                y = t->right; t->right = y->left; y->left = t; t = y;
            }
            if (t->right == NULL) break;
            l->right = t; l = t; t = t->right;
        } else break;
    }
    l->right=t->left; r->left=t->right; t->left=N.right; t->right=N.left;
    return t;
}

sTree * insert(key_type i, sTree * t) {
    /* Insert i into the sTree t, unless it's already there.    */
    /* Return a pointer to the resulting sTree.                 */
    sTree * new;
    
    new = (sTree *) malloc (sizeof (sTree));
    if (new == NULL) {
        printf("Ran out of space\n");
        exit(1);
    }
    assign_key(new, i);
    new->value = 1;
    if (t == NULL) {
        new->left = new->right = NULL;
        return new;
    }
    t = splay(i,t);
    if (key_cmp(i, t->key) < 0) {
        new->left = t->left;
        new->right = t;
        t->left = NULL;
        t->value = 1 + node_value(t->right);
    } else if (key_cmp(i, t->key) > 0) {
        new->right = t->right;
        new->left = t;
        t->right = NULL;
        t->value = 1 + node_value(t->left);
    } else { /* We get here if it's already in the sTree */
        /* Don't add it again                      */
        free_node(new);
        assert (t->value == 1 + node_value(t->left) + node_value(t->right));
        return t;
    }
    new->value = 1 + node_value(new->left) + node_value(new->right);
    return new;
}

sTree * splay_delete(key_type i, sTree * t) {
    /* Deletes i from the sTree if it's there.               */
    /* Return a pointer to the resulting sTree.              */
    sTree * x;
    if (t==NULL) return NULL;
    long root_value = t->value;
    t = splay(i,t);
    if (key_cmp(i, t->key) == 0) {               /* found it */
        if (t->left == NULL) {
            x = t->right;
        } else {
            x = splay(i, t->left);      // i is the largest element in left sub sTree,
                                        // so the new splay sTree does not have right sub sTree 
            x->right = t->right;
        }
        free_node(t);
        if (x != NULL) {
            x->value = root_value-1;
        }
        return x;
    }
    return t;                         /* It wasn't there */
}


//sTree *find_node(key_type e, sTree *t) {
//    /* Returns a pointer to the node in the sTree with the given value.  */
//    /* Returns NULL if there is no such node.                          */
//    /* Does not change the sTree.  To guarantee logarithmic behavior,   */
//    /* the node found here should be splayed to the root.              */
//    T lsize;
////    if ((value < 0) || (value >= node_value(t))) return NULL;
//    for (;;) {
//        if (key_cmp(e, t->key)>0){
//            
//        }
//        lsize = node_value(t->left);
//        if (r < lsize) {
//            t = t->left;
//        } else if (r > lsize) {
//            r = r - lsize -1;
//            t = t->right;
//        } else {
//            return t;
//        }
//    }
//}
void free_sTree(sTree* t)
{
    if(t==NULL) return;
    free_sTree(t->right);
    free_sTree(t->left);
    free_node(t);
}
void print_sTree(sTree * t, int d) {
    //printf("%p\n",t);
    int i;
    if (t == NULL) return;
    print_sTree(t->right, d+1);
    for (i=0; i<d; i++) printf("  ");
    printf("%lld(%ld)\n", t->key, t->value);
    print_sTree(t->left, d+1);
}

void check_sTree(sTree* t){
    /* check the value of sTree node, make sure all values are correct in the sTree */
    if (t==NULL)
        return;
    assert(node_value(t) == node_value(t->left)+node_value(t->right)+1);
    if (t->left != NULL)
        check_sTree(t->left);
    if (t->right != NULL)
        check_sTree(t->right);
}


#ifdef __cplusplus
}
#endif



// int splay_main() {
//     /* A sample use of these functions.  Start with the empty sTree,         */
//     /* insert some stuff into it, and then delete it                        */
//     sTree * root;
//     int i;
//     root = NULL;              /* the empty sTree */
//     for (i = 0; i < 1024; i++) {
//         root = insert((541*i) & (1023), root);
//         check_sTree(root);
//     }
//     for (i = 0; i < 1024; i++) {
//         root = splay_delete((541*i) & (1023), root);
//         check_sTree(root);
//     }

//     printf("root=%p\n", root);
//     root = insert(1, root);
//     check_sTree(root);
//     root = insert(3, root);
//     root = insert(5, root);
//     root = insert(12, root);
//     root = insert(8, root);
//     root = insert(6, root);
//     print_sTree(root, 3);
//     check_sTree(root);
//     root = splay_delete(2, root);
//     printf("\n");
//     print_sTree(root, 3);
//     free_sTree(root);
    
//     return (0);
// }

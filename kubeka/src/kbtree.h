
#ifndef H_KBTREE
#define H_KBTREE

#ifdef __cplusplus
extern "C" {
#endif

   // Returns a set of the nodes, along with a count of duplicates in the
   // `nduplicates` parameter.
   ds_array_t *kbtree_coalesce (ds_array_t *nodes, size_t *nduplicates,
                                size_t *nerrors, size_t *nwarnings);

   // Using the given node as the root of an instantiated tree, perform
   // all the variable substitutions.
   void kbtree_eval (kbnode_t *root, ds_array_t *nodes,
                     size_t *nerrors, size_t *nwarnings);



#ifdef __cplusplus
};
#endif


#endif



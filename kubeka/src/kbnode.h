         /* ****************************************************** *
          * Copyright ©2024 Run Data Systems,  All rights reserved *
          *                                                        *
          * This content is the exclusive intellectual property of *
          * Run Data Systems, Gauteng, South Africa.               *
          *                                                        *
          * See the LICENSE file for more information.             *
          *                                                        *
          * ****************************************************** */


#ifndef H_KBNODE
#define H_KBNODE

typedef struct kbnode_t kbnode_t;

#define KBNODE_TYPE_PERIODIC     "periodic"
#define KBNODE_TYPE_JOB          "job"
#define KBNODE_TYPE_ENTRYPOINT   "entrypoint"

#define KBNODE_KEY_FNAME      "_FILENAME"
#define KBNODE_KEY_LINE       "_LINE"
#define KBNODE_KEY_ID         "ID"
#define KBNODE_KEY_MESSAGE    "MESSAGE"
#define KBNODE_KEY_JOBS       "JOBS"
#define KBNODE_KEY_EXEC       "EXEC"
#define KBNODE_KEY_EMITS      "EMITS"
#define KBNODE_KEY_HANDLES    "HANDLES"
#define KBNODE_KEY_ROLLBACK   "ROLLBACK"

#define KBNODE_FLAG_INSTANTIATED    (1 >> 0)

#ifdef __cplusplus
extern "C" {
#endif

   // A comparison function to non-recursively compare one node against another.
   int kbnode_cmp (const kbnode_t *lhs, size_t, const kbnode_t *rhs, size_t);

   // Get and set the flags for the specified node
   uint64_t kbnode_flags (kbnode_t *node);
   void kbnode_flags_set (kbnode_t *node, uint64_t flags);

   // Write the node out to the file descriptor provided (used during development)
   void kbnode_dump (const kbnode_t *node, FILE *outf, size_t level);

   // Return an array of all the keys in a node. Caller must free the returned
   // array only, and not each element in the returned array.
   const char **kbnode_keys (const kbnode_t *node);

   // Return all the jobs/handlers of a node.
   const ds_array_t *kbnode_jobs (const kbnode_t *node);
   const ds_array_t *kbnode_handlers (const kbnode_t *node);

   // Delete a node and all its jobs and handlers (only instantiated nodes have
   // jobs and handlers).
   void kbnode_del (kbnode_t *node);

   // Get the source filename and line number where this node was declared. On error
   // the `fname` is set to a constant string, line number is set to zero and
   // `false` is returned.
   //
   // On success true is returned. On both success and failre the caller must not
   // free the `fname` that is set.
   bool kbnode_get_srcdef (const kbnode_t *node, const char **id, const char **fname,
                           size_t *line);

   // Get the first value for the specified key. The caller must not free the
   // returned value.
   const char *kbnode_getvalue_first (const kbnode_t *node, const char *key);
   // Get all the values for the specified key. The caller must not free the
   // returned array, nor any element of that array.
   const char **kbnode_getvalue_all (const kbnode_t *node, const char *key);

   bool kbnode_set_single (kbnode_t *node, const char *key, size_t index,
                           char *newvalue);

   // Caller must ensure that *dst is initialised with ds_array_new(). The number of
   // errors is stored in `nerrors` and the number of warnings is stored in
   // `nwarnings`.
   //
   // Returns `true` on success anf `false` on failure.
   bool kbnode_read_file (ds_array_t *dst, const char *fname,
                          size_t *nerrors, size_t *nwarnings);

   // Performs a basic sanity check on the specified node. This is not recursive and
   // will ignore child nodes. Records the number of errors and number of warnings
   // in the parameters specified.
   void kbnode_check (kbnode_t *node, size_t *errors, size_t *warnings);

   // Return an array of nodes that match any of the types specified in `type, ...`.
   // Note that the final parameter must be NULL.
   ds_array_t *kbnode_filter_types (const ds_array_t *nodes, const char *type, ...);

   // Return an array of nodes that have any of the keys specified in `keyname, ...`.
   // Note that the final parameter must be NULL.
   ds_array_t *kbnode_filter_keyname (const ds_array_t *nodes, const char *keyname, ...);

   // Return an array of nodes that handle the specified signals. Note that the
   // final parameter must be NULL.
   ds_array_t *kbnode_filter_handlers (const ds_array_t *nodes, const char **signals);

   // Instantiate and return the specified node `src`. The returned node will be a
   // tree which contains all child nodes as specified in the value of the `JOBS[]`
   // symbol.
   //
   // The `all` array is used to locate nodes referenced by `src`. The number of
   // errors and warnings are populated in the respective parameters.
   kbnode_t *kbnode_instantiate (const kbnode_t *src, ds_array_t *all,
                                 size_t *errors, size_t *warnings);

   // Return the first occurrence of `value` in the symbol table. If the value
   // does not exist, parents are checked recursively.
   //
   // If symbol is not found, returns NULL and does not change the value of nerrors.
   // On error, returns NULL *and* populates the `nerrors` argument. Because this is
   // a recursive function, the caller must set *nerrors to zero before calling this
   // function.
   const char **kbnode_resolve (const kbnode_t *node, const char *symbol);

#ifdef __cplusplus
};
#endif


#endif



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
#define KBNODE_KEY_EMIT       "EMIT"

#ifdef __cplusplus
extern "C" {
#endif

   int kbnode_cmp (const kbnode_t *lhs, size_t, const kbnode_t *rhs, size_t);

   uint64_t kbnode_flags (kbnode_t *node);
   void kbnode_flags_set (kbnode_t *node, uint64_t flags);
   void kbnode_dump (const kbnode_t *node, FILE *outf);
   void kbnode_del (kbnode_t *node);
   bool kbnode_get_srcdef (kbnode_t *node, const char **fname, size_t *line);

   const char *kbnode_get (kbnode_t *node, const char *key);

   // Returns number of errors encountered. Caller must ensure that
   // *dst is initialised with ds_array_new();
   bool kbnode_read_file (ds_array_t *dst, const char *fname,
                          size_t *nerrors, size_t *nwarnings);

   void kbnode_check (kbnode_t *node, size_t *errors, size_t *warnings);

   ds_array_t *kbnode_filter_types (ds_array_t *nodes, const char *type, ...);
   ds_array_t *kbnode_filter_varname (ds_array_t *nodes, const char *varname, ...);

   kbnode_t *kbnode_instantiate (const kbnode_t *src, ds_array_t *all,
                                 size_t *errors, size_t *warnings);


#ifdef __cplusplus
};
#endif


#endif



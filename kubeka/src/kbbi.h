
#ifndef H_KBBI
#define H_KBBI

typedef char *(kbbi_fptr_t) (const char *name, const char *params,
                             const kbnode_t *node,
                             size_t *nerrors, const char *fname, size_t line);

#ifdef __cplusplus
extern "C" {
#endif

   kbbi_fptr_t *kbbi_fptr (const char *name);

   int kbbi_launch (const char *name, ds_array_t *nodes,
                    size_t *nerrors, size_t *nwarnings);


#ifdef __cplusplus
};
#endif


#endif



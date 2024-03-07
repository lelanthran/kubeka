
#ifndef H_KBBI
#define H_KBBI

typedef char *(kbbi_fptr_t) (const char *name, const char *params,
                             const kbnode_t *node,
                             size_t *nerrors, const char *fname, size_t line);

struct kbbi_thread_t {
   volatile sig_atomic_t *endflag; // TODO: Use cmpxchange calls
   volatile pthread_t tid; // TODO: Use cmpxchange calls
   kbnode_t *root;
   int retcode;
   volatile bool completed; // TODO: Use cmpxchange calls
};

#ifdef __cplusplus
extern "C" {
#endif

   kbbi_fptr_t *kbbi_fptr (const char *name);

   int kbbi_launch (const char *name, ds_array_t *nodes,
                    size_t *nerrors, size_t *nwarnings);

   bool kbbi_thread_launch (struct kbbi_thread_t *th);
   void kbbi_thread_cancel (struct kbbi_thread_t *th);


#ifdef __cplusplus
};
#endif


#endif



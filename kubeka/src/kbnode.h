
#ifndef H_KBNODE
#define H_KBNODE

typedef struct kbnode_t kbnode_t;

#ifdef __cplusplus
extern "C" {
#endif

   void kbnode_dump (const kbnode_t *node, FILE *outf);
   void kbnode_del (kbnode_t *node);

   // Returns number of errors encountered. Caller must ensure that
   // *dst is initialised with ds_array_new();
   size_t kbnode_read_file (ds_array_t **dst, const char *fname);

#ifdef __cplusplus
};
#endif


#endif



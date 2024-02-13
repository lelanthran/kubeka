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

#ifdef __cplusplus
extern "C" {
#endif

   void kbnode_dump (const kbnode_t *node, FILE *outf);
   void kbnode_del (kbnode_t *node);

   // Returns number of errors encountered. Caller must ensure that
   // *dst is initialised with ds_array_new();
   size_t kbnode_read_file (ds_array_t **dst, const char *fname);

   ds_array_t *kbnode_filter_type (ds_array_t *nodes, const char *type);


#ifdef __cplusplus
};
#endif


#endif



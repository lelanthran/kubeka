
#ifndef H_KBEXEC
#define H_KBEXEC

#ifdef __cplusplus
extern "C" {
#endif

   // Note that dst and dstlen are used unchanged, so if dst was already allocated
   // with length of dstlen bytes, it will be reused until the output of the command
   // exceeds dstlen, at which point it will be reallocated.
   int kbexec_shell (const char *node_fname, size_t node_line,
                     const char *command, char **dst, size_t *dstlen);


#ifdef __cplusplus
};
#endif


#endif



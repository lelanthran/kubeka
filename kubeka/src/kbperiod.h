
#ifndef H_KBPERIOD
#define H_KBPERIOD

typedef struct kbperiod_t kbperiod_t;

#ifdef __cplusplus
extern "C" {
#endif

   kbperiod_t *kbperiod_parse (const char *src);
   void kbperiod_del (kbperiod_t *kbp);

   uint64_t kbperiod_remaining (kbperiod_t *kbp);
   void kbperiod_reset (kbperiod_t *kbp);




#ifdef __cplusplus
};
#endif


#endif



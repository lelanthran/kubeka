
#ifndef H_KBSYM
#define H_KBSYM

typedef struct kbsymtab_t kbsymtab_t;

#ifdef __cplusplus
extern "C" {
#endif

   void kbsymtab_dump (const kbsymtab_t *s, FILE *outf, size_t level);

   kbsymtab_t *kbsymtab_new (void);

   void kbsymtab_del (kbsymtab_t *st);

   kbsymtab_t *kbsymtab_copy (kbsymtab_t *st);

   const char **kbsymtab_get (const kbsymtab_t *st, const char *key);

   const char **kbsymtab_keys (const kbsymtab_t *st);

   bool kbsymtab_set (const char *fname, size_t lc, bool force,
                      kbsymtab_t *st, const char *key, const char *value);

   bool kbsymtab_append (const char *fname, size_t lc, bool force,
                         kbsymtab_t *st, const char *key, char *value);

   bool kbsymtab_exists (kbsymtab_t *st, const char *key);

   const char *kbsymtab_get_string (const kbsymtab_t *st, const char *key);
   int64_t kbsymtab_get_int (const kbsymtab_t *st, const char *key);

#ifdef __cplusplus
};
#endif


#endif



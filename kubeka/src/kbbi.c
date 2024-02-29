#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>


#include "ds_array.h"
#include "ds_str.h"

#include "kbnode.h"
#include "kbbi.h"
#include "kbutil.h"


#define INCPTR(x)    do {\
   (x) = (x) + 1;\
} while (0)


#define KBBI_FUNC(x) \
   static char *x (const char *name, const char *params, const kbnode_t *node,\
                   size_t *nerrors, const char *fname, size_t line)

KBBI_FUNC(bi_setenv);
KBBI_FUNC(bi_getenv);

static const struct {
   const char *name;
   kbbi_fptr_t *fptr;
} functions[] = {
   { "setenv",    bi_setenv },
   { "getenv",    bi_getenv },
};


kbbi_fptr_t *kbbi_fptr (const char *name)
{
   size_t nfuncs = sizeof functions/sizeof functions[0];
   for (size_t i=0; i<nfuncs; i++) {
      if ((strcmp (name, functions[i].name)) == 0) {
         return functions[i].fptr;
      }
   }
   return NULL;
}


#define KBBI_DEFINITION(x) \
   static char *x (const char *name, const char *params, const kbnode_t *node,\
                   size_t *nerrors, const char *fname, size_t line)

KBBI_FUNC(bi_setenv)
{
   (void)name;
   (void)node;

   // Some very platform-specific code goes here, due to how hard it is
   // (some would say impossible) to set environments without leaking
   // memory.
   errno = 0;

   char **p = kbutil_strsplit (params, '=');
   if (!p) {
      KBERROR ("OOM splitting params [%s]\n", params);
      INCPTR (*nerrors);
      return NULL;
   }

   if (!p[0] || !p[1]) {
      KBPARSE_ERROR (fname, line, "Failed to extract NAME=VALUE for setenv()\n"
                                  "[%s] %p %p\n",
                                  params, p[0], p[1]);
      INCPTR (*nerrors);
      kbutil_strarray_del (p);
      return NULL;
   }

   int rc = setenv (p[0], p[1], 1);
   if (rc != 0) {
      KBPARSE_ERROR (fname, line, "Failed to set env [%s] = [%s]\n",
               p[0], p[1]);
      INCPTR (*nerrors);
   }

   kbutil_strarray_del (p);
   return ds_str_dup ("");
}

KBBI_FUNC(bi_getenv)
{
   (void)name;
   (void)node;
   (void)nerrors;
   (void)fname;
   (void)line;

   char *value = getenv (params);
   if (!value) {
      value = "";
   }
   return ds_str_dup (value);
}



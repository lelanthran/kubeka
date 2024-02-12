         /* ****************************************************** *
          * Copyright ©2024 Run Data Systems,  All rights reserved *
          *                                                        *
          * This content is the exclusive intellectual property of *
          * Run Data Systems, Gauteng, South Africa.               *
          *                                                        *
          * See the LICENSE file for more information.             *
          *                                                        *
          * ****************************************************** */


#include <stdio.h>
#include <stdlib.h>

#include "ds_str.h"

#include "kbutil.h"

char *kbutil_file_read (const char *fname)
{
   FILE *inf = fopen (fname, "r");
   if (!inf) {
      fprintf (stderr, "Failed to open [%s] for reading: %m\n", fname);
      return NULL;
   }

   char *ret = NULL;
   fseek (inf, 0, SEEK_END);
   long len = ftell (inf);
   fseek (inf, 0, SEEK_SET);

   if (!(ret = calloc (len + 1, 1))) {
      fprintf (stderr, "Failed to allocate memory %li bytes\n", len);
      fclose (inf);
   }

   size_t nbytes = 0;
   if ((nbytes = fread (ret, 1, len, inf)) != (size_t)len) {
      fprintf (stderr, "Failed to read entire file: %zu/%li read\n", nbytes, len);
      free (ret);
      ret = NULL;
   }

   fclose (inf);
   return ret;
}

void kbutil_strarray_del (char **sa)
{
   for (size_t i=0; sa && sa[i]; i++) {
      free (sa[i]);
   }
   free (sa);
}

char *kbutil_strarray_format (char **sa)
{
   char *ret = ds_str_dup ("[ ");
   const char *delim = "";
   for (size_t i=0; sa && sa[i]; i++) {
      if (!(ds_str_append (&ret, delim, sa[i], " ", NULL))) {
         KBERROR ("OOM error formatting array\n");
         free (ret);
         return NULL;
      }
      delim = ",";
   }
   if (!(ds_str_append (&ret, "]", NULL))) {
      free (ret);
      return NULL;
   }

   return ret;
}

size_t kbutil_strarray_length (char **sa)
{
   size_t ret = 0;
   if (!sa || !sa[0])
      return 0;

   for (size_t i=0; sa[i]; i++) {
      ret++;
   }
   return ret;
}

char **kbutil_strarray_append (char ***dst, char *s)
{
   if (!dst || !s) {
      return NULL;
   }

   size_t n = kbutil_strarray_length (*dst);
   char **tmp = realloc (*dst, (sizeof *dst) * (n + 2));
   if (!tmp) {
      return NULL;
   }

   tmp [n] = s;
   tmp [n + 1] = NULL;
   *dst = tmp;
   return *dst;
}


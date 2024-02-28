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
#include <stdbool.h>
#include <string.h>

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

char *kbutil_strarray_format (const char **sa)
{
   size_t nvalues = kbutil_strarray_length (sa);
   if (nvalues == 0) {
      return ds_str_dup ("");
   }
   if (nvalues == 1) {
      return ds_str_dup (sa[0]);
   }


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

size_t kbutil_strarray_length (const char **sa)
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

   size_t n = kbutil_strarray_length ((const char **)*dst);
   char **tmp = realloc (*dst, (sizeof *dst) * (n + 2));
   if (!tmp) {
      return NULL;
   }

   tmp [n] = s;
   tmp [n + 1] = NULL;
   *dst = tmp;
   return *dst;
}


bool kbutil_test (const char *name, int expected_rc,
                  const char *ifname, const char *ofname, const char *efname,
                  int (*testfunc) (const char *, const char *))
{
   bool error = true;
   int rc = testfunc (ifname, ofname);
   char *output = NULL, *expected = NULL;

   if (rc != expected_rc) {
      fprintf (stderr, "Error while running [%s:%s]\n", name, ifname);
      goto cleanup;
   }

   if (!(output = kbutil_file_read (ofname))) {
      fprintf (stderr, "Failed to read in file %s: %m\n", ofname);
      goto cleanup;
   }
   if (!(expected = kbutil_file_read (efname))) {
      fprintf (stderr, "Failed to read in file %s: %m\n", efname);
      goto cleanup;
   }

   size_t olen = strlen (output);
   size_t elen = strlen (expected);

   if (olen != elen
         || (memcmp (output, expected, strlen (output))) != 0) {
      fprintf (stderr, "Unexpected output, run:\ndiff %s %s\n",
               ofname, efname);
      goto cleanup;
   }

   error = false;
cleanup:

   free (output);
   free (expected);
   printf ("TEST (%s): %s\n", name, error ? "failed" : "passed");
   return !error;
}

char **kbutil_strsplit (const char *src, char delim)
{
   char *tmp = ds_str_dup (src);
   if (!tmp) {
      KBERROR ("OOM error duplicating string [%s]\n", src);
      return NULL;
   }

   char *tok = NULL;
   char delims[2] = { delim, 0 };
   char *p1 = tmp;
   char *saveptr = NULL;
   size_t nitems = 1;

   while ((tok = strtok_r (p1, delims, &saveptr))) {
      p1 = NULL;
      nitems++;
   }

   char **ret = calloc (nitems, sizeof *ret);
   if (!ret) {
      KBERROR ("OOM allocating result\n");
      free (tmp);
      return NULL;
   }

   free (tmp);
   if (!(tmp = ds_str_dup (src))) {
      KBERROR ("OOM duplicating string (twice) [%s]\n", src);
      free (ret);
      return NULL;
   }

   p1 = tmp;
   size_t i = 0;

   while ((tok = strtok_r (p1, delims, &saveptr))) {
      p1 = NULL;
      if (!(ret[i++] = ds_str_dup (tok))) {
         KBERROR ("OOM copying token [%s]\n", tok);
         kbutil_strarray_del (ret);
         free (tmp);
         return NULL;
      }
   }

   free (tmp);
   return ret;
}

char **kbutil_strarray_copy (const char **src)
{
   bool error = true;

   char **ret = NULL;
   size_t nitems = 1;
   for (size_t i=0; src && src[i]; i++) {
      nitems++;
   }

   if (!(ret = calloc (nitems, sizeof *ret))) {
      KBERROR ("OOM creating string array\n");
      goto cleanup;
   }

   for (size_t i=0; src && src[i]; i++) {
      if (!(ret[i] = ds_str_dup (src[i]))) {
         KBERROR ("OOM creating string\n");
         goto cleanup;
      }
   }

   error = false;

cleanup:
   if (error) {
      kbutil_strarray_del (ret);
      ret = NULL;
   }
   return ret;
}


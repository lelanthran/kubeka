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


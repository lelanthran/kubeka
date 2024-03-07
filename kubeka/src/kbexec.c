#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

#include "kbexec.h"
#include "kbutil.h"

bool resize_buffer (char **dst, size_t *len, size_t index)
{
   index += 2;
   if (index < *len) {
      return true;
   }

   size_t newsize = index * 2;
   char *tmp = realloc (*dst, newsize + 2);
   if (!tmp) {
      return false;
   }

   *dst = tmp;
   *len = newsize; // exclude +2; better to reallocate earlier than necessary
   return true;
}

int kbexec_shell (const char *node_fname, size_t node_line,
                  const char *command, char **dst, size_t *dstlen)
{
   int ret = EXIT_FAILURE;

   FILE *inp = popen (command, "r");
   if (!inp) {
      KBPARSE_ERROR (node_fname, node_line, "Failed to execute: [%s]: %m\n",
                     command);
      return EXIT_FAILURE;
   }

   *dstlen = 0;
   size_t index = 0;
   if (!(resize_buffer (dst, dstlen, index))) {
      KBIERROR ("Failed to resize array");
      goto cleanup;
   }
   (*dst)[0] = 0;

   int c;
   while ((c = fgetc (inp)) != EOF) {
      if (!(resize_buffer (dst, dstlen, index))) {
         KBIERROR ("OOM reallocating %zu bytes when executing [%s]\n",
                   index, command);
         goto cleanup;
      }
      (*dst)[index] = (char)c;
      (*dst)[index+1] = 0;
      index++;
   }

   ret = EXIT_SUCCESS;

cleanup:
   if (inp) {
      errno = 0;
      ret = pclose (inp);
      if (ret || errno) {
         ret = EXIT_FAILURE;
      }
   }

   return ret;
}


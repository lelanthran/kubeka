         /* ****************************************************** *
          * Copyright ©2024 Run Data Systems,  All rights reserved *
          *                                                        *
          * This content is the exclusive intellectual property of *
          * Run Data Systems, Gauteng, South Africa.               *
          *                                                        *
          * See the file COPYRIGHT for more information.           *
          *                                                        *
          * ****************************************************** */



#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "ds_array.h"

#include "kbnode.h"
#include "kbutil.h"

#define FNAME_INPUT     "./tests/input/kbnode.txt"
#define FNAME_OUTPUT    "./tests/output/kbnode.txt"
#define FNAME_EXPECTED  "./tests/expected/kbnode.txt"

int main (void)
{
   int ret = EXIT_FAILURE;
   errno = 0;

   char *output = NULL;
   char *expected = NULL;

   ds_array_t *nodes = ds_array_new ();
   FILE *outf = fopen (FNAME_OUTPUT, "w");

   if (!nodes) {
      fprintf (stderr, "Failed to create node collection\n");
      goto cleanup;
   }

   if (!outf) {
      fprintf (stderr, "Failed to open [%s] for writing: %m\n", FNAME_INPUT);
      goto cleanup;
   }

   if ((kbnode_read_file (&nodes, FNAME_INPUT))) {
      fprintf (stderr, "Failed to parse [%s]: %m\n", FNAME_INPUT);
      goto cleanup;
   }
   size_t nnodes = ds_array_length (nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_dump ((kbnode_t *)ds_array_get (nodes, i), outf);
   }

   fclose (outf);
   outf = NULL;

   if (!(output = kbutil_file_read (FNAME_OUTPUT))) {
      fprintf (stderr, "Failed to read in file %s: %m\n", FNAME_OUTPUT);
      goto cleanup;
   }
   if (!(expected = kbutil_file_read (FNAME_EXPECTED))) {
      fprintf (stderr, "Failed to read in file %s: %m\n", FNAME_EXPECTED);
      goto cleanup;
   }


   if ((memcmp (output, expected, strlen (output))) != 0) {
      fprintf (stderr, "Unexpected output, run a diff on [%s] and [%s]\n",
               FNAME_EXPECTED, FNAME_OUTPUT);
      goto cleanup;
   }


   ret = EXIT_SUCCESS;
cleanup:
   free (output);
   free (expected);
   nnodes = ds_array_length (nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_t *node = ds_array_get (nodes, i);
      kbnode_del (node);
   }
   ds_array_del (nodes);

   if (outf) {
      fclose (outf);
   }
   printf ("TEST (%s:%s): %s\n", __FILE__, __func__,
         ret == EXIT_SUCCESS ? "passed" : "failed");
   return ret;
}


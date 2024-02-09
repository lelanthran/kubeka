
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "ds_array.h"

#include "kbnode.h"

#define FNAME_INPUT     "./tests/input/kbnode.txt"
#define FNAME_OUTPUT    "./tests/output/kbnode.txt"
#define FNAME_EXPECTED  "./tests/expected/kbnode.txt"

// TODO: Move this, as well as the strarray functions, into a utility module.
char *file_read (const char *fname)
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

   if (!(output = file_read (FNAME_OUTPUT))) {
      fprintf (stderr, "Failed to read in file %s: %m\n", FNAME_OUTPUT);
      goto cleanup;
   }
   if (!(expected = file_read (FNAME_EXPECTED))) {
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
   return ret;
}


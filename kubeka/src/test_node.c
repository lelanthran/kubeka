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

#define PARSER_IN        "./tests/input/kbnode.txt"
#define PARSER_OUT       "./tests/output/kbnode.txt"
#define PARSER_EXP       "./tests/expected/kbnode.txt"

static int t_parser (const char *ifname, const char *ofname)
{
   int ret = EXIT_FAILURE;

   ds_array_t *nodes = ds_array_new ();
   FILE *outf = fopen (ofname, "w");

   if (!nodes) {
      fprintf (stderr, "Failed to create node collection\n");
      goto cleanup;
   }

   if (!outf) {
      fprintf (stderr, "Failed to open [%s] for writing: %m\n", ofname);
      goto cleanup;
   }

   if ((kbnode_read_file (&nodes, ifname))) {
      fprintf (stderr, "Failed to parse [%s]: %m\n", ifname);
      goto cleanup;
   }

   size_t nnodes = ds_array_length (nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_dump ((kbnode_t *)ds_array_get (nodes, i), outf);
   }

   ret = EXIT_SUCCESS;
cleanup:
   if (outf) {
      fclose (outf);
   }

   nnodes = ds_array_length (nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_t *node = ds_array_get (nodes, i);
      kbnode_del (node);
   }
   ds_array_del (nodes);

   return ret;
}

int main (void)
{
   int ret = 0;
   static struct {
      const char *name;
      const char *ifname;
      const char *ofname;
      const char *efname;
      int (*testfunc) (const char *, const char *);
   } tests[] = {
      { "test_parser", PARSER_IN, PARSER_OUT, PARSER_EXP, t_parser },
   };

   for (size_t i=0; i<sizeof tests/sizeof tests[0]; i++) {
      ret |= kbutil_test (tests[i].name,
                          tests[i].ifname, tests[i].ofname, tests[i].efname,
                          tests[i].testfunc);
   }

   return ret;
}


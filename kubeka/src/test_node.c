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
#include <stdint.h>
#include <stdbool.h>

#include "ds_array.h"

#include "kbnode.h"
#include "kbutil.h"

#define PARSER_IN       "./tests/input/kbnode.txt"
#define PARSER_OUT      "./tests/output/kbnode.txt"
#define PARSER_EXP      "./tests/expected/kbnode.txt"

#define RO_IN           "./tests/input/kbnode-set-ro.txt"
#define RO_OUT          "./tests/output/kbnode-set-ro.txt"
#define RO_EXP          "./tests/expected/kbnode-set-ro.txt"

#define F1_IN           "./tests/input/kbnode-f1.txt"
#define F1_OUT          "./tests/output/kbnode-f1.txt"
#define F1_EXP          "./tests/expected/kbnode-f1.txt"

static void dump_nodelist (ds_array_t *nodes, FILE *outf)
{
   size_t nnodes = ds_array_length (nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_dump ((kbnode_t *)ds_array_get (nodes, i), outf);
   }
}

static int t_parser (const char *ifname, const char *ofname)
{
   int ret = EXIT_FAILURE;

   ds_array_t *nodes = ds_array_new ();
   FILE *outf = fopen (ofname, "w");

   if (!outf) {
      fprintf (stderr, "Failed to open [%s] for writing: %m\n", ofname);
      goto cleanup;
   }

   if (!nodes) {
      fprintf (stderr, "Failed to create node collection\n");
      goto cleanup;
   }

   size_t e = 0;
   size_t w = 0;
   if (!(kbnode_read_file (nodes, ifname, &e, &w))) {
      fprintf (stderr, "Failed to parse [%s]: %m\n", ifname);
      fprintf (stderr, "Errors: %zu, warnings: %zu\n", e, w);
      goto cleanup;
   }

   dump_nodelist (nodes, outf);

   ret = EXIT_SUCCESS;
cleanup:
   if (outf) {
      fclose (outf);
   }

   size_t nnodes = ds_array_length (nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_t *node = ds_array_get (nodes, i);
      kbnode_del (node);
   }
   ds_array_del (nodes);

   return ret;
}

static int t_filter (const char *ifname, const char *ofname)
{
   int ret = EXIT_FAILURE;

   ds_array_t *nodes = ds_array_new ();

   ds_array_t *f1 = NULL, *f2 = NULL, *f3 = NULL;

   FILE *outf = fopen (ofname, "w");

   if (!outf) {
      fprintf (stderr, "Failed to open [%s] for writing: %m\n", ofname);
      goto cleanup;
   }

   if (!nodes) {
      fprintf (stderr, "Failed to create node collection\n");
      goto cleanup;
   }

   size_t e = 0;
   size_t w = 0;
   if (!(kbnode_read_file (nodes, ifname, &e, &w))) {
      fprintf (stderr, "Failed to parse [%s]: %m\n", ifname);
      fprintf (stderr, "Errors: %zu, warnings: %zu\n", e, w);
      goto cleanup;
   }

   // Print out all the nodes
   dump_nodelist (nodes, outf);

   fprintf (outf, "f1 ====================================\n");
   if (!(f1 = kbnode_filter_types (nodes, "periodic", NULL))) {
      fprintf (stderr, "Failed to filter by type\n");
      goto cleanup;
   }
   dump_nodelist (f1, outf);

   fprintf (outf, "f2 ====================================\n");
   if (!(f2 = kbnode_filter_keyname (nodes, "for_filter", NULL))) {
      fprintf (stderr, "Failed to filter by keyname\n");
      goto cleanup;
   }
   dump_nodelist (f2, outf);


   ret = EXIT_SUCCESS;
cleanup:
   if (outf) {
      fclose (outf);
   }

   size_t nnodes = ds_array_length (nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_t *node = ds_array_get (nodes, i);
      kbnode_del (node);
   }
   ds_array_del (nodes);
   ds_array_del (f1);
   ds_array_del (f2);
   ds_array_del (f3);

   return ret;
}


int main (void)
{
   int ret = 0;
   static struct {
      const char *name;
      int expected_rc;
      const char *ifname;
      const char *ofname;
      const char *efname;
      int (*testfunc) (const char *, const char *);
   } tests[] = {
{ "test_parser",  EXIT_SUCCESS, PARSER_IN, PARSER_OUT, PARSER_EXP, t_parser },
{ "test_ro",      EXIT_FAILURE, RO_IN, RO_OUT, RO_EXP, t_parser },
{ "filter1",      EXIT_SUCCESS, F1_IN, F1_OUT, F1_EXP, t_filter },
   };

   for (size_t i=0; i<sizeof tests/sizeof tests[0]; i++) {
      ret |= kbutil_test (tests[i].name,
                          tests[i].expected_rc,
                          tests[i].ifname, tests[i].ofname, tests[i].efname,
                          tests[i].testfunc);
   }

   return ret;
}


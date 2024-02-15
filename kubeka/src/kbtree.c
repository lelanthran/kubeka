#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ds_array.h"
#include "ds_set.h"


#include "kbtree.h"
#include "kbutil.h"
#include "kbsym.h"
#include "kbnode.h"

static int cmpnode (const void *lhs, size_t lhslen,
                    const void *rhs, size_t rhslen)
{
   const kbnode_t *l = lhs;
   const kbnode_t *r = rhs;
   return kbnode_cmp (l, lhslen, r, rhslen);
}

static void array_push (const void *node, size_t len, void *array)
{
   (void)len;
   ds_array_t *a = array;
   if (!(ds_array_ins_tail (a, (void *)node))) {
      KBERROR ("Error inserting node into array (full node dump follows)\n");
      kbnode_dump (node, stderr);
   }
}


ds_array_t *kbtree_coalesce (ds_array_t *nodes, size_t *nduplicates,
                             size_t *nerrors, size_t *nwarnings)
{
   bool error = true;

   *nerrors = 0;
   *nwarnings = 0;

   ds_set_t *set = NULL;
   size_t ndups = 0;
   size_t nnodes = ds_array_length (nodes);

   if (!nnodes) {
      return ds_array_new ();
   }

   if (!(set = ds_set_new (cmpnode, nnodes))) {
      KBERROR ("OOM error creating set for nodes.\n");
      *nerrors = (*nerrors) + 1;
      return NULL;
   }

   for (size_t i=0; i<nnodes; i++) {
      kbnode_t *node = ds_array_get (nodes, i);
      kbnode_t *existing = ds_set_find (set, node, 0);
      if (existing) {
         const char *node_src_fname = NULL;
         size_t node_src_line = 0;
         const char *existing_src_fname = NULL;
         size_t existing_src_line = 0;

         if (!(kbnode_get_srcdef (existing, &existing_src_fname, &existing_src_line))
               || !(kbnode_get_srcdef (node, &node_src_fname, &node_src_line))) {
            KBERROR ("Failed to retrieve file/line number information for duplicate"
                     " nodes, aborting\n");
            *nerrors = (*nerrors) + 1;
            goto cleanup;
         }

         fprintf (stderr, "Duplicate node found ...\n");
         fprintf (stderr, "Node-1 found in [%s:%zu]\n",
                  existing_src_fname, existing_src_line);
         fprintf (stderr, "Node-2 found in [%s:%zu]\n",
                  node_src_fname, node_src_line);
         fprintf (stderr, "=== Node-1 dump follows: === \n");
         kbnode_dump (existing, stderr);
         fprintf (stderr, "=== Node-2 dump follows: === \n");
         kbnode_dump (node, stderr);
         ndups++;
         *nerrors = (*nerrors) + 1;
      }
      int added = ds_set_add (set, node, 0);
      if (added < 0) {
         KBERROR ("Failed to add following node to set of all nodes\n");
         kbnode_dump (node, stderr);
      }
      if (added == 0) {
         KBERROR ("Failed to add duplicated node\n");
      }
   }

   if (nduplicates) {
      *nwarnings = (*nwarnings) + ndups;
      *nduplicates = ndups;
   }

   error = false;
cleanup:
   if (error) {
      ds_set_del (set);
      set = NULL;
   }

   ds_array_t *ret = ds_array_new ();
   if (!ret) {
      KBERROR ("OOM creating deduplicated list\n");
      return NULL;
   }

   ds_set_iterate (set, array_push, ret);
   ds_set_del (set);
   return ret;
}



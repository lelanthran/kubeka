#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ds_array.h"
#include "ds_set.h"
#include "ds_str.h"


#include "kbutil.h"
#include "kbsym.h"
#include "kbnode.h"
#include "kbtree.h"
#include "kbbi.h"

#define INCPTR(x)    do {\
   (x) = (x) + 1;\
} while (0)

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

static char *exec_builtin (char *ref, const kbnode_t *node, size_t *nerrors,
                           const char *fname, size_t line)
{
   char *func = &ref[2];
   char *params = strchr (func, ' ');
   if (!params) {
      KBPARSE_ERROR (fname, line, "Failed to parse '%s' as a function call\n",
            func);
      INCPTR (*nerrors);
      return NULL;
   }
   *params++ = 0;
   char *end = strchr (params, ')');
   if (!end) {
      KBPARSE_ERROR (fname, line, "Failed to find end of function call in [%s]\n",
            params);
      INCPTR (*nerrors);
      return NULL;
   }
   *end = 0;


   kbbi_fptr_t *fptr = kbbi_fptr (func);
   if (!fptr) {
      KBPARSE_ERROR (fname, line, "Call to undefined function '%s'\n",
               func);
      INCPTR (*nerrors);
      return NULL;
   }

   char *ret = fptr (func, params, node, nerrors, fname, line);
   *end = ')';
   *(params - 1) = ' ';
   KBERROR ("function [%s] returned [%s]\n", ref, ret);
   return ret;
}

static char *resolve (char *ref, const kbnode_t *node, size_t *nerrors,
                      const char *fname, size_t line)
{
   if ((strchr (ref, ' '))) {
      return exec_builtin (ref, node, nerrors, fname, line);
   }

   char *start = &ref[2];
   char *end = strchr (start, ')');
   if (!end) {
      KBPARSE_ERROR (fname, line, "Missing terminating ')' in symbol reference\n");
      INCPTR (*nerrors);
      return NULL;
   }

   *end = 0;
   const char **values = kbnode_resolve (node, start);
   *end = ')';

   if (!values) {
      KBPARSE_ERROR (fname, line, "Failed to find values for symbol %s\n",
               start);
      INCPTR (*nerrors);
      return NULL;
   }

   return kbutil_strarray_format (values);
}

static char *find_next_ref (const char *src, size_t *nerrors,
                            const char *fname, size_t line)
{
   char *ret = NULL;
   char *start = strstr (src, "$(");
   if (!start) {
      return NULL;
   }

   char *end = strchr (&start[2], ')');
   if (!end) {
      KBPARSE_ERROR (fname, line, "Missing terminating ')' in symbol reference\n");
      INCPTR (*nerrors);
      return NULL;
   }
   end++;

   size_t nbytes = end - start;
   nbytes++;
   if (!(ret = calloc (nbytes, 1))) {
      INCPTR (*nerrors);
      return NULL;
   }

   size_t index = 0;
   while (start < end) {
      ret[index++] = *start++;
   }

   return ret;
}

void kbtree_eval (kbnode_t *root, size_t *nerrors, size_t *nwarnings)
{
   const char **keys = kbnode_keys (root);
   const char *fname;
   size_t line;

   if (!(kbnode_get_srcdef (root, &fname, &line))) {
      KBERROR ("Failed to get node filename and line number information\n");
      INCPTR (*nerrors);
      goto cleanup;
   }

   if (!keys) {
      KBPARSE_ERROR (fname, line, "Failed to get node symbols\n");
      INCPTR (*nerrors);
      goto cleanup;
   }


   // for each $key in the symtab {
   //    for each $value in the array of values from $key {
   //       value = perform_subst (value, node)
   //       node_set_symbol
   //       }
   //    }
   // }
   //

   for (size_t i=0; keys[i]; i++) {
      const char **values = kbnode_getvalue_all (root, keys[i]);
      if (!values) {
         KBPARSE_ERROR (fname, line, "Failed to get values for symbol %s\n",
                  keys[i]);
         INCPTR (*nwarnings); // TODO: Should this be an error?
         continue;
      }

      for (size_t j=0; values[j]; j++) {

         if (!(kbnode_get_srcdef (root, &fname, &line))) {
            KBERROR ("Failed to get node filename and line number information\n");
            INCPTR (*nerrors);
            goto cleanup;
         }

         char *newvalue = ds_str_dup (values[j]);
         char *ref = NULL;
         size_t errors = 0;

         while (!errors && (ref = find_next_ref (newvalue, &errors, fname, line))) {
            char *resolved = resolve (ref, root, &errors, fname, line);
            if (errors || !resolved) {
               free (ref);
               ref = NULL;
               break;
            }
            char *tmp = ds_str_strsubst (newvalue, ref, resolved, NULL);
            free (ref);
            ref = NULL;
            free (resolved);
            resolved = NULL;
            if (!tmp) {
               KBERROR ("OOM performing substitution\n");
               break;
            }
            free (newvalue);
            newvalue = tmp;
        }
         *nerrors = (*nerrors) + errors;
         if (errors) {
            KBPARSE_ERROR (fname, line, "Aborting due to errors\n");
            free (newvalue);
            break;
         }

         kbnode_set_single (root, keys[i], j, newvalue);
      }
   }

   const ds_array_t *children = kbnode_children (root);
   size_t nchildren = ds_array_length (children);
   for (size_t i=0; i<nchildren; i++) {
      kbnode_t *child = ds_array_get (children, i);
      kbtree_eval (child, nerrors, nwarnings);
   }

cleanup:
   free (keys);
}



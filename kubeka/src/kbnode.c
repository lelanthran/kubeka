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
#include <ctype.h>
#include <stdarg.h>

#include "ds_array.h"
#include "ds_hmap.h"
#include "ds_str.h"

#include "kbnode.h"
#include "kbsym.h"
#include "kbutil.h"

/* ***********************************************************
 * Misc utility functions
 */
/* ***********************************************************
 * Node functions
 */

enum node_type_t {
   node_type_UNKNOWN = 0,
   node_type_CRON,
   node_type_JOB,
};

static const struct {
   enum node_type_t type;
   const char *name;
} node_type_names[] = {
   { node_type_UNKNOWN, "unknown node type"  },
   { node_type_CRON,    "cron"               },
   { node_type_JOB,     "job"                },
};

static const char *node_type_name (enum node_type_t type)
{

   static char unknown[45];

   for (size_t i=0; i<sizeof node_type_names / sizeof node_type_names[0]; i++) {
      if (node_type_names[i].type == type) {
         return node_type_names[i].name;
      }
   }
   snprintf (unknown, sizeof unknown, "Unknown node type: %i\n", type);
   return unknown;
}

static enum node_type_t node_type_type (const char *name)
{
   for (size_t i=0; i<sizeof node_type_names / sizeof node_type_names[0]; i++) {
      if ((strcmp (node_type_names[i].name, name)) == 0) {
         return node_type_names[i].type;
      }
   }
   return node_type_UNKNOWN;
}

struct kbnode_t {
   enum node_type_t type;
   kbsymtab_t *symtab;
   kbnode_t *parent;
   ds_array_t *children;
};

static size_t node_find_child (kbnode_t *node, kbnode_t *child)
{
   if (!node || !child) {
      return (size_t)-1;
   }
   size_t nchildren = ds_array_length (node->children);
   for (size_t i=0; i<nchildren; i++) {
      if ((ds_array_get (node->children, i)) == child) {
         return i;
      }
   }
   return (size_t)-1;
}

static void node_del (kbnode_t **node)
{
   if (!node || !*node)
      return;

   // Remove this node from parent's list of children
   size_t me = node_find_child ((*node)->parent, *node);
   if (me != (size_t)-1) {
      ds_array_rm ((*node)->parent->children, me);
   }

   // Recursively delete all children
   for (size_t i=0; i<ds_array_length ((*node)->children); i++) {
      kbnode_t *child = ds_array_get ((*node)->children, i);
      node_del (&child);
   }
   ds_array_del ((*node)->children);

   // Clear out the symbol table
   kbsymtab_del ((*node)->symtab);

   free (*node);
}

static kbnode_t *node_new (const char *fname, size_t line,
                           const char *typename, kbnode_t *parent)
{
   bool error = true;
   enum node_type_t type = node_type_type (typename);
   if (type == node_type_UNKNOWN) {
      KBPARSE_ERROR (fname, line,
            "Attempt to create node of unknown type: '%s'\n", typename);
      return NULL;
   }

   errno = ENOMEM;

   kbnode_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      goto cleanup;
   }

   if (!(ret->symtab = kbsymtab_new ())) {
      goto cleanup;
   }

   if (!(ret->children = ds_array_new ())) {
      goto cleanup;
   }

   if (parent) {
      ret->parent = parent;
      if (!(ds_array_ins_tail (parent->children, ret))) {
         goto cleanup;
      }
   }
   ret->type = type;

   error = false;
cleanup:
   if (error) {
      node_del (&ret);
   }
   return ret;
}


/* ***********************************************************
 * Helper functions
 */

static bool parse_nv (char **name, char **value, char *line, const char *delim)
{
   // TODO: All occurrences of `strchr()` must be replaced with
   // mystrchr which returns the first non-escaped character.
   char *tmp = strstr (line, delim);
   if (!tmp) {
      return false;
   }

   *tmp = 0;
   tmp += strlen (delim);

   if (!(*name = ds_str_dup (line))
         || !(*value = ds_str_dup (tmp))) {
      free (*name); *name = NULL;
      free (*value); *value = NULL;
      return false;
   }

   ds_str_trim (*name);
   ds_str_trim (*value);

   return true;
}


/* ***********************************************************
 * Public functions
 */


void kbnode_dump (const kbnode_t *node, FILE *outf)
{
   if (!outf)
      outf = stdout;
   if (!node) {
      fprintf (outf, "NULL node object!\n");
      return;
   }

   fprintf (outf, "Node [%s] with parent [%p]\n",
         node_type_name(node->type), node->parent);

   kbsymtab_dump (node->symtab, outf);

   size_t nchildren = ds_array_length (node->children);
   for (size_t i=0; i<nchildren; i++) {
      kbnode_dump ((kbnode_t *)(ds_array_get (node->children, i)), outf);
   }
}

void kbnode_del (kbnode_t *node)
{
   node_del (&node);
}

size_t kbnode_read_file (ds_array_t **dst, const char *fname)
{
   errno = 0;
   size_t wcount = 0;

   char *line = NULL;
   char *tmp = NULL;
   size_t lc = 0;
   FILE *inf = NULL;

   kbnode_t *current = NULL;

   char *name = NULL, *value = NULL;

   if (!(inf = fopen (fname, "r"))) {
      KBERROR ("Failed to open [%s] for reading: %m\n", fname);
      goto cleanup;
   }

   // 1MB ought to be enough for everyone!
   static const int line_len = 1024 * 1024;

   if (!(line = malloc (line_len))) {
      KBPARSE_ERROR (fname, lc, "Failed to allocate buffer for input\n");
      errno = ENOMEM;
      goto cleanup;
   }

   while (!feof (inf) && !ferror (inf) && fgets (line, line_len, inf)) {
      lc++;
      if ((tmp = strchr (line, '\r'))) {
         KBPARSE_ERROR (fname, lc,
               "Carriage return (\\r) detected on line %zu\n", lc);
         errno = EILSEQ; // TODO: Maybe Windows needs a different error?
         goto cleanup;
      }

      // TODO: All occurrences of `strchr()` must be replaced with
      // mystrchr which returns the first non-escaped character.
      if ((tmp = strchr (line, '#'))) {
         // TODO: Check for escaped character
         *tmp = 0;
      }
      if ((tmp = strchr (line, '\n'))) {
         *tmp = 0;
      }

      ds_str_trim (line);
      // Empty line, ignore
      if (line[0] == 0) {
         continue;
      }

      // Use a number of different ways to classify the input line into
      // one of the following types:
      // [text]            A new node of type 'text'
      // name = value      Variable assignment
      // name!             Unset a variable
      // name += value     Append value to variable `name`
      //

      // Do we have a new node
      if (line[0] == '[') {

         // TODO: All occurrences of `strchr()` must be replaced with
         // mystrchr which returns the first non-escaped character.
         char *tmp = strchr (line, ']');
         if (!tmp) {
            KBPARSE_ERROR (fname, lc, "Mangled input [%s]\n", line);
            wcount++;
            goto cleanup;
         }
         *tmp = 0;

         if (!(current = node_new (fname, lc, &line[1], NULL))) {
            KBPARSE_ERROR (fname, lc,
                  "Node creation attempt failure near: '%s'\n", &line[1]);
            wcount++;
            goto cleanup;
         }

         if (!(ds_array_ins_tail (*dst, current))) {
            KBERROR ("OOM appending new node %s to collection\n", line);
            wcount++;
            goto cleanup;
         }

         // Nothing more to do ...
         continue;
      }

      free (name); name = NULL;
      free (value); value = NULL;

      // Perform a concatenation with the existing value
      if ((strstr (line, "+="))) {
         if (!(parse_nv (&name, &value, line, "+="))) {
            KBPARSE_ERROR (fname, lc, "Failed to read name/value pair near '%s'\n",
                  line);
            wcount++;
            goto cleanup;
         }

         if (!(kbsymtab_append (fname, lc, current->symtab, name, value))) {
            KBPARSE_ERROR (fname, lc, "Failed to append value to '%s': \n",
                  name);
            wcount++;
            goto cleanup;
         }

         // Nothing more to do, continue reading file
         continue;
      }

      // TODO: All occurrences of `strchr()` must be replaced with
      // mystrchr which returns the first non-escaped character.
      // Perform a simple assignment/creation/replacement
      if ((strchr (line, '='))) {
         if (!(parse_nv (&name, &value, line, "="))) {
            KBPARSE_ERROR (fname, lc, "Failed to read name/value pair near '%s'\n",
                  line);
            wcount++;
            goto cleanup;
         }
         if (!(kbsymtab_set (fname, lc, current->symtab, name, value))) {
            KBPARSE_ERROR (fname, lc, "Error setting value for '%s': %s\n",
                  name, value);
            errno = ENOTSUP;
            wcount++;
            goto cleanup;
         }

         // Nothing more
         continue;
      }

      // If we get here, it means that the line was not matched to any
      // pattern we support
      KBPARSE_ERROR (fname, lc, "Unrecognised pattern in input '%s'\n", line);
      wcount++;
   }

cleanup:
   free (line);
   free (name);
   free (value);

   if (inf) {
      fclose (inf);
   }

   if (wcount) {
      size_t nelements = ds_array_length (*dst);
      for (size_t i=0; i<nelements; i++) {
         kbnode_del (ds_array_get (*dst, i));
      }
      ds_array_del (*dst);
      *dst = NULL;
   }

   return wcount;
}



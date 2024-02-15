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
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>

#include "ds_set.h"
#include "ds_array.h"
#include "ds_hmap.h"
#include "ds_str.h"

#include "kbnode.h"
#include "kbsym.h"
#include "kbutil.h"

#define BUILTIN_FNAME      "_FILENAME"
#define BUILTIN_LINE       "_LINE"
#define BUILTIN_ID         "ID"
#define BUILTIN_MESSAGE    "MESSAGE"
#define BUILTIN_JOBS       "JOBS"
#define BUILTIN_EXEC       "EXEC"
#define BUILTIN_EMIT       "EMIT"

#define INCPTR(x)    do {\
   (x) = (x) + 1;\
} while (0)

/* ***********************************************************
 * Misc utility functions
 */
/* ***********************************************************
 * Node functions
 */

enum node_type_t {
   node_type_UNKNOWN = 0,
   node_type_PERIODIC,
   node_type_JOB,
   node_type_ENTRYPOINT,
};

static const struct {
   enum node_type_t type;
   const char *name;
} node_type_names[] = {
   { node_type_UNKNOWN,       "unknown node type"     },
   { node_type_PERIODIC,      KBNODE_TYPE_PERIODIC    },
   { node_type_JOB,           KBNODE_TYPE_JOB         },
   { node_type_ENTRYPOINT,    KBNODE_TYPE_ENTRYPOINT  },
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
   uint64_t flags;
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

static const kbnode_t *node_findbyid (ds_array_t *all, const char *id)
{
   size_t n = ds_array_length (all);
   for (size_t i=0; i< n; i++) {
      const kbnode_t *node = ds_array_get (all, i);
      const char *node_id = kbsymtab_get_string (node->symtab, BUILTIN_ID);
      if ((strcmp (node_id, id)) == 0) {
         return node;
      }
   }
   return NULL;
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
   char sline[45];
   kbnode_t *ret = NULL;

   snprintf (sline, sizeof sline, "%zu", line);

   if (type == node_type_UNKNOWN) {
      KBPARSE_ERROR (fname, line,
            "Attempt to create node of unknown type: '%s'\n", typename);
      goto cleanup;
   }

   errno = ENOMEM;

   ret = calloc (1, sizeof *ret);
   if (!ret) {
      goto cleanup;
   }

   if (!(ret->symtab = kbsymtab_new ())) {
      goto cleanup;
   }

   if (!(kbsymtab_set (fname, line, true, ret->symtab, BUILTIN_FNAME, fname)) ||
         !((kbsymtab_set (fname, line, true, ret->symtab,BUILTIN_LINE, sline)))) {
      KBPARSE_ERROR (fname, line, "Failed to set default vars\n");
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

static const char *node_filename (const kbnode_t *node)
{
   return kbsymtab_get_string (node->symtab, BUILTIN_FNAME);
}

static int64_t node_line (const kbnode_t *node)
{
   return kbsymtab_get_int (node->symtab, BUILTIN_LINE);
}

static kbnode_t *node_instantiate (const kbnode_t *src, kbnode_t *parent,
                                   ds_array_t *all,
                                   size_t *errors, size_t *warnings)
{
   bool error = true;
   const char **jobs = NULL;
   const kbnode_t *ref = NULL;

   // 1. Create a new node (fname and line don't matter here, it will be set
   // below anyway during the cloning of the symbol table)
   kbnode_t *ret = node_new ("", 0, node_type_name (src->type), parent);

   // 2. Copy the symbol table (easier to just recreate it)
   kbsymtab_del (ret->symtab);
   if (!(ret->symtab = kbsymtab_copy (src->symtab))) {
      KBERROR ("OOM creating new symbol table\n");
      INCPTR (*errors);
      goto cleanup;
   }

   // 3, Find all the children
   if (!(jobs = kbsymtab_get (src->symtab, BUILTIN_JOBS))) {
      // No JOBS[] to create children from, ignore
      error = false;
      goto cleanup;
   }

   // 4. Recursively create all children
   for (size_t i=0; jobs && jobs[i]; i++) {
      if (!(ref = node_findbyid (all, jobs[i]))) {
         KBPARSE_ERROR (node_filename (src), node_line (src),
               "Failed to find reference to job [%s]\n", jobs[i]);
         INCPTR (*errors);
         continue;
      }

      if (!(node_instantiate (ref, ret, all, errors, warnings))) {
         KBPARSE_ERROR (node_filename (src), node_line (src),
                  "Failed to instantiate child %zu [%s]\n", i, jobs[i]);
         INCPTR (*errors);
      }
   }

   error = false;

cleanup:
   if (error) {
      node_del (&ret);
      ret = NULL;
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


int kbnode_cmp (const kbnode_t *lhs, size_t l1, const kbnode_t *rhs, size_t l2)
{
   (void)l1;
   (void)l2;
   if (!lhs || !rhs)
      return -1;

   const char **lhs_id = kbsymtab_get (lhs->symtab, "ID");
   const char **rhs_id = kbsymtab_get (rhs->symtab, "ID");
   if (!lhs_id || !lhs_id[0] || !rhs_id || !rhs_id[0])
      return 1;

   int ret = strcmp (lhs_id[0], rhs_id[0]);

   return ret;
}


uint64_t kbnode_flags (kbnode_t *node)
{
   return node->flags;
}

void kbnode_flags_set (kbnode_t *node, uint64_t flags)
{
   node->flags = flags;
}

void kbnode_dump (const kbnode_t *node, FILE *outf)
{
   if (!outf)
      outf = stdout;
   if (!node) {
      fprintf (outf, "NULL node object!\n");
      return;
   }

   fprintf (outf, "Node [%s] with parent [%p]: 0x%" PRIx64 "\n",
         node_type_name(node->type), node->parent, node->flags);

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

bool kbnode_get_srcdef (kbnode_t *node, const char **fname, size_t *line)
{
   *line = 0;
   *fname = NULL;

   if (!node) {
      return false;
   }

   const char **f = kbsymtab_get (node->symtab, BUILTIN_FNAME);
   const char **l = kbsymtab_get (node->symtab, BUILTIN_LINE);
   if (!f || !f[0] || !l || !l[0]) {
      return false;
   }

   sscanf (l[0], "%zu", line);
   *fname = f[0];
   return true;
}


bool kbnode_read_file (ds_array_t *dst, const char *fname,
                       size_t *nerrors, size_t *nwarnings)
{
   errno = 0;

   *nerrors = 0;
   *nwarnings = 0;

   char *line = NULL;
   char *tmp = NULL;
   size_t lc = 0;
   FILE *inf = NULL;
   size_t nnodes = 0;

   kbnode_t *current = NULL;

   char *name = NULL, *value = NULL;

   if (!(inf = fopen (fname, "r"))) {
      KBERROR ("Failed to open [%s] for reading: %m\n", fname);
      *nerrors = (*nerrors) + 1;
      goto cleanup;
   }

   // 1MB ought to be enough for everyone!
   static const int line_len = 1024 * 1024;

   if (!(line = malloc (line_len))) {
      KBPARSE_ERROR (fname, lc, "Failed to allocate buffer for input\n");
      errno = ENOMEM;
      *nerrors = (*nerrors) + 1;
      goto cleanup;
   }

   while (!feof (inf) && !ferror (inf) && fgets (line, line_len, inf)) {
      lc++;
      if ((tmp = strchr (line, '\r'))) {
         KBPARSE_ERROR (fname, lc,
               "Carriage return (\\r) detected on line %zu\n", lc);
         errno = EILSEQ; // TODO: Maybe Windows needs a different error?
         *nerrors = (*nerrors) + 1;
         current = NULL;
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
            *nerrors = (*nerrors) + 1;
            goto cleanup;
         }
         *tmp = 0;

         if (!(current = node_new (fname, lc, &line[1], NULL))) {
            KBPARSE_ERROR (fname, lc,
                  "Node creation attempt failure near: '%s'\n", &line[1]);
            *nerrors = (*nerrors) + 1;
            goto cleanup;
         }

         if (!(ds_array_ins_tail (dst, current))) {
            KBERROR ("OOM appending new node %s to collection\n", line);
            *nerrors = (*nerrors) + 1;
            goto cleanup;
         }
         nnodes++;

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
            *nerrors = (*nerrors) + 1;
            current = NULL;
            goto cleanup;
         }

         if (!current) {
            KBPARSE_ERROR (fname, lc,
                  "`+=` found before any node is defined with [<node>]\n");
            *nerrors = (*nerrors) + 1;
            current = NULL;
            goto cleanup;
         }

         if (!(kbsymtab_append (fname, lc, false, current->symtab, name, value))) {
            KBPARSE_ERROR (fname, lc, "Failed to append value to '%s': \n",
                  name);
            *nerrors = (*nerrors) + 1;
            current = NULL;
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
            *nerrors = (*nerrors) + 1;
            current = NULL;
            goto cleanup;
         }

         if (!current) {
            KBPARSE_ERROR (fname, lc,
                  "`+=` found before any node is defined with [<node>]\n");
            *nerrors = (*nerrors) + 1;
            current = NULL;
            goto cleanup;
         }

         if (!(kbsymtab_set (fname, lc, false, current->symtab, name, value))) {
            KBPARSE_ERROR (fname, lc, "Error setting value for '%s' to '%s'\n",
                  name, value);
            errno = ENOTSUP;
            *nerrors = (*nerrors) + 1;
            current = NULL;
            goto cleanup;
         }

         // Nothing more
         continue;
      }

      // If we get here, it means that the line was not matched to any
      // pattern we support
      KBPARSE_WARN (fname, lc, "Unrecognised pattern in input '%s'\n", line);
      *nwarnings = (*nwarnings) + 1;
   }

cleanup:
   free (line);
   free (name);
   free (value);

   if (inf) {
      fclose (inf);
   }

   if (!nnodes) {
      KBPARSE_WARN (fname, lc, "No nodes found in file\n");
      *nwarnings = (*nwarnings) + 1;
   }

   if (*nerrors) {
      kbnode_del (current);
   }

   if (*nerrors || *nwarnings) {
      return false;
   }
   return true;
}

static bool node_filter_func_types (const void *element, void *param)
{
   const kbnode_t *node = element;
   const char **types = param;

   for (size_t i=0; types && types[i]; i++) {
      enum node_type_t type = node_type_type (types[i]);
      if (node->type == type) {
         return true;
      }
   }
   return false;
}

static bool node_filter_names (const void *element, void *param)
{
   const kbnode_t *node = element;
   const char **names = param;

   for (size_t i=0; names && names[i]; i++) {
      if (kbsymtab_exists (node->symtab, names[i])) {
         return true;
      }
   }
   return false;
}

static char **collect_args (const char *a1, va_list ap)
{
   char *tmp = (char *)a1;
   char **ret = NULL;
   va_list ap2;
   va_copy (ap2, ap);

   size_t nelements = 0;
   do {
      nelements++;
   } while ((tmp = va_arg (ap, char *)));

   if (!(ret = calloc (nelements + 1, sizeof *ret))) {
      KBERROR ("OOM error allocating array of %zu entries\n", nelements + 1);
      va_end (ap2);
      return NULL;
   }

   tmp = (char *)a1;
   size_t i = 0;
   do {
      ret[i++] = tmp;
   } while ((tmp = va_arg (ap2, char *)));

   va_end (ap2);

   return ret;
}

void kbnode_check (kbnode_t *node, size_t *errors, size_t *warnings)
{
   static const char *required[] = {
      BUILTIN_ID, BUILTIN_MESSAGE,
   };

   if (!node) {
      KBERROR ("NULL kbnode_t object found!\n");
      INCPTR(*errors);
      return;
   }

   const char *fname = kbsymtab_get_string (node->symtab, BUILTIN_FNAME);
   int64_t line = kbsymtab_get_int (node->symtab, BUILTIN_LINE);

   if (!fname || line == LLONG_MAX) {
      KBERROR ("No filename/line information for node [%s:%" PRIi64 "]\n",
            fname, line);
      INCPTR(*warnings);
   }

   for (size_t i=0; i<sizeof required/sizeof required[0]; i++) {
      if (!(kbsymtab_exists (node->symtab, required[i]))) {
         KBPARSE_WARN (fname, line, "Missing required key '%s'\n", required[i]);
         INCPTR(*errors);
      }
   }

   // Possibly refactor this into a different function. For now this is fine
   // as we only have a single set of XOR keys to check
   int exec = kbsymtab_exists (node->symtab, BUILTIN_EXEC) ? 1 : 0;
   int emit = kbsymtab_exists (node->symtab, BUILTIN_EMIT) ? 1 : 0;
   int jobs = kbsymtab_exists (node->symtab, BUILTIN_JOBS) ? 1 : 0;
   if ((exec + emit + jobs) != 1) {
      KBPARSE_ERROR (fname, line,
               "Exactly one of EXEC, EMIT or JOBS must be specified\n");
      INCPTR(*errors);
   }
}

ds_array_t *kbnode_filter_types (ds_array_t *nodes, const char *type, ...)
{
   va_list ap;
   va_start (ap, type);
   char **types = collect_args (type, ap);
   ds_array_t *ret = ds_array_filter (nodes, node_filter_func_types, types);
   free (types);
   return ret;
}

ds_array_t *kbnode_filter_varname (ds_array_t *nodes, const char *varname, ...)
{
   va_list ap;
   va_start (ap, varname);
   char **names = collect_args (varname, ap);
   ds_array_t *ret = ds_array_filter (nodes, node_filter_names, names);
   free (names);
   return ret;
}


kbnode_t *kbnode_instantiate (const kbnode_t *src, ds_array_t *all,
                              size_t *errors, size_t *warnings)
{
   if (!src) {
      KBERROR ("NULL node found\n");
      INCPTR(*errors);
      return NULL;
   }

   kbnode_t *ret = node_instantiate (src, NULL, all, errors, warnings);
   if (!ret) {
      KBPARSE_ERROR (node_filename (src), node_line (src),
            "Failed to instantiate node\n");
      return NULL;
   }

   // 1. Get a list of all the keys
   // 2. Perform a substitution of each key with the value


   return ret;
}


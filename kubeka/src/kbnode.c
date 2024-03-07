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

#define INCPTR(x)    do {\
   (x) = (x) + 1;\
} while (0)

/* ***********************************************************
 * Misc utility functions
 */
/* ***********************************************************
 * Node functions
 */

enum childtype_t {
   childtype_NONE = 0,
   childtype_JOB,
   childtype_HANDLER,
};

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
   ds_array_t *jobs;
   ds_array_t *handlers;
   uint64_t flags;
};

static size_t find_node (ds_array_t *nodelist, kbnode_t *node)
{
   if (!nodelist || !node) {
      return (size_t)-1;
   }
   size_t nnodes = ds_array_length (nodelist);
   for (size_t i=0; i<nnodes; i++) {
      if ((ds_array_get (nodelist, i)) == node) {
         return i;
      }
   }
   return (size_t)-1;
}

static size_t node_find_job (kbnode_t *node, kbnode_t *job)
{
   return node ? find_node (node->jobs, job) : (size_t)-1;
}

static size_t node_find_handler (kbnode_t *node, kbnode_t *handler)
{
   return node ? find_node (node->handlers, handler) : (size_t)-1;
}


static const kbnode_t *node_findbyid (ds_array_t *all, const char *id)
{
   size_t n = ds_array_length (all);
   for (size_t i=0; i< n; i++) {
      const kbnode_t *node = ds_array_get (all, i);
      const char *node_id = kbsymtab_get_string (node->symtab, KBNODE_KEY_ID);
      if ((strcmp (node_id, id)) == 0) {
         return node;
      }
   }
   return NULL;
}

static const kbnode_t *node_findparent (const kbnode_t *node, const char *id)
{
   if (!node)
      return NULL;

   fprintf (stderr, "Checking [%s] for a parent with id [%s]\n",
            kbsymtab_get_string (node->symtab, KBNODE_KEY_ID),
            id);
   if ((strcmp (kbsymtab_get_string (node->symtab, KBNODE_KEY_ID), id)) == 0) {
      return node;
   }

   return node_findparent (node->parent, id);
}

struct djobs_t {
   enum childtype_t childtype;
   const char *id;
};

static struct djobs_t *node_find_dependent_jobs (const kbnode_t *node,
                                                 ds_array_t *all,
                                                 size_t *nerrors)
{
   if (!node || !all) {
      return NULL;
   }

   bool error = true;
   const char *fname = NULL, *id = NULL;
   size_t line = 0;
   struct djobs_t *ret = NULL;

   ds_array_t *sighandlers = NULL;

   if (!(kbnode_get_srcdef (node, &id, &fname, &line))) {
      INCPTR (*nerrors);
      KBXERROR ("Failed to get node information\n");
      return NULL;
   }

   const char **jobs = kbsymtab_get (node->symtab, KBNODE_KEY_JOBS);
   const char **signals = kbsymtab_get (node->symtab, KBNODE_KEY_EMITS);

   // Doing it using a filter might miss the fact that some of the signals
   // don't have handlers. Have to search for a handler for each individual
   // signal ensures that every emitted signal has at least one handler
   ds_array_t *handlers = ds_array_new ();
   if (!handlers) {
      KBIERROR ("OOM allocating new array\n");
      INCPTR (*nerrors);
      goto cleanup;
   }

   size_t njobs = kbutil_strarray_length (jobs);
   size_t nstrings = kbutil_strarray_length (signals) + njobs;
   if (!(ret = calloc (nstrings +1, sizeof *ret))) {
      INCPTR (*nerrors);
      goto cleanup;
   }
   // Store the result of jobs search (easy)
   size_t idx = 0;
   for (size_t i=0; i < njobs; i++) {
      ret[idx].id = jobs[i];
      ret[idx].childtype = childtype_JOB;
      idx++;
   }

   // For each signal, find all the nodes that handle that signal
   for (size_t i=0; signals && signals[i]; i++) {
      const char *sigs[] = {
         signals[i],
         NULL,
      };

      sighandlers = kbnode_filter_handlers (all, sigs);
      if (!sighandlers) {
         KBIERROR ("OOM filtering signals into array\n");
         INCPTR (*nerrors);
         goto cleanup;
      }
      size_t nsighandlers = ds_array_length (sighandlers);
      if (!nsighandlers) {
         KBPARSE_ERROR (fname, line, "Node [%s] signal [%s] is unhandled\n",
                  id, signals[i]);
         INCPTR (*nerrors);
         goto cleanup;
      }

      for (size_t j=0; j<nsighandlers; j++) {
         if (!(ds_array_ins_tail (handlers, ds_array_get (sighandlers, j)))) {
            KBIERROR ("OOM inserting signal handler node into handlers\n");
            INCPTR (*nerrors);
            goto cleanup;
         }
      }
      ds_array_del (sighandlers);
      sighandlers = NULL;
   }

   size_t nhandlers = ds_array_length (handlers);

   // For each handler, store its ID and childtype
   for (size_t i=0; i<nhandlers; i++) {
      kbnode_t *handler = ds_array_get (handlers, i);
      const char *id = kbnode_getvalue_first (handler, KBNODE_KEY_ID);
      if (!id) {
         INCPTR (*nerrors);
         goto cleanup;
      }
      ret[idx].id = id;
      ret[idx].childtype = childtype_HANDLER;
      idx++;
   }

   error = false;

cleanup:
   ds_array_del (handlers);
   ds_array_del (sighandlers);
   if (error) {
      free (ret);
      ret = NULL;
   }
   return ret;
}

static void node_del (kbnode_t *node)
{
   if (!node)
      return;

   // Remove this node from parent's list of jobs and handlers
   if (node->parent) {
      size_t me = -1;
      if ((me = node_find_job (node->parent, node)) != (size_t)-1) {
         ds_array_rm (node->parent->jobs, me);
      }
      if ((me = node_find_handler (node->parent, node)) != (size_t)-1) {
         ds_array_rm (node->parent->jobs, me);
      }
   }

   // Recursively delete all jobs and handlers
   ds_array_fptr (node->jobs, (void (*) (void *))node_del);
   ds_array_fptr (node->handlers, (void (*) (void *))node_del);
   ds_array_del (node->jobs);
   ds_array_del (node->handlers);

   // Clear out the symbol table
   kbsymtab_del (node->symtab);

   free (node);
}

static kbnode_t *node_new (const char *fname, size_t line,
                           const char *typename,
                           kbnode_t *parent, enum childtype_t childtype)
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

   if (!(kbsymtab_set (fname, line, true, ret->symtab, KBNODE_KEY_FNAME, fname)) ||
         !((kbsymtab_set (fname, line, true, ret->symtab,KBNODE_KEY_LINE, sline)))) {
      KBPARSE_ERROR (fname, line, "Failed to set default vars\n");
      goto cleanup;
   }

   if (!(ret->jobs = ds_array_new ()) || !(ret->handlers = ds_array_new ())) {
      goto cleanup;
   }

   // Attach to parent, if a parent is specified.
   if (parent) {
      ret->parent = parent;
      void *inserted = NULL;
      switch (childtype) {
         case childtype_JOB:
            inserted = ds_array_ins_tail (parent->jobs, ret);
            break;

         case childtype_HANDLER:
            inserted = ds_array_ins_tail (parent->handlers, ret);
            break;

         case childtype_NONE:
            break;
      }
      if (!inserted) {
         goto cleanup;
      }
   }
   ret->type = type;

   error = false;
cleanup:
   if (error) {
      node_del (ret);
   }
   return ret;
}

static const char *node_filename (const kbnode_t *node)
{
   return kbsymtab_get_string (node->symtab, KBNODE_KEY_FNAME);
}

static int64_t node_line (const kbnode_t *node)
{
   return kbsymtab_get_int (node->symtab, KBNODE_KEY_LINE);
}

static kbnode_t *node_instantiate (const kbnode_t *src,
                                   kbnode_t *parent, enum childtype_t childtype,
                                   ds_array_t *all,
                                   size_t *errors, size_t *warnings)
{
   bool error = true;
   struct djobs_t *jobs = NULL;
   const kbnode_t *ref = NULL;

   fprintf (stderr, "Instantiating [%s] as child of [%s]\n",
         kbsymtab_get_string (src->symtab, KBNODE_KEY_ID),
         parent ? kbnode_getvalue_first (parent, KBNODE_KEY_ID) : "NULL");

   // 1. Create a new node (fname and line don't matter here, it will be set
   // below anyway during the cloning of the symbol table)
   kbnode_t *ret = node_new ("", 0, node_type_name (src->type), parent, childtype);

   // 2. Copy the symbol table (easier to just recreate it)
   kbsymtab_del (ret->symtab);
   if (!(ret->symtab = kbsymtab_copy (src->symtab))) {
      KBIERROR ("OOM creating new symbol table\n");
      INCPTR (*errors);
      goto cleanup;
   }

   // 3, Find all the references to jobs and handlers
   if (!(jobs = node_find_dependent_jobs (src, all, errors))) {
      // No JOBS[] to create jobs from, no signals to emit, so nothing to do.
      error = false;
      goto cleanup;
   }

   // 4. Recursively create all jobs
   for (size_t i=0; jobs && jobs[i].id && jobs[i].childtype; i++) {

      if (!(ref = node_findbyid (all, jobs[i].id))) {
         KBPARSE_ERROR (node_filename (src), node_line (src),
               "Failed to find reference to job [%s]\n", jobs[i].id);
         INCPTR (*errors);
         continue;
      }

      // If the job node is also an ancestor, then we give up - nodes
      // cannot reference each other recursively.
      const kbnode_t *ancestor = node_findparent (parent, jobs[i].id);
      if (ancestor) {
         KBPARSE_ERROR (node_filename (src), node_line (src),
               "Reference-cycle found. Node [%s] recursively calls node [%s]\n",
               kbsymtab_get_string (src->symtab, KBNODE_KEY_ID),
               kbsymtab_get_string (ancestor->symtab, KBNODE_KEY_ID));
         fprintf (stderr, "Node 1:\n");
         kbnode_dump (parent, stderr, 0);
         fprintf (stderr, "Node 2:\n");
         kbnode_dump (ancestor, stderr, 0);
         INCPTR(*errors);
         goto cleanup;
      }

      if (!(node_instantiate (ref, ret, jobs[i].childtype, all, errors, warnings))) {
         KBPARSE_ERROR (node_filename (src), node_line (src),
                  "Failed to instantiate job %zu [%s]\n", i, jobs[i].id);
         INCPTR (*errors);
         goto cleanup;
      }
   }

   ret->flags |= KBNODE_FLAG_INSTANTIATED;

   error = false;

cleanup:
   free (jobs);
   if (error) {
      node_del (ret);
      ret = NULL;
   }

   return ret;
}




/* ***********************************************************
 * Helper functions
 */

static bool parse_nv (char **name, char **value, char *line, const char *delim)
{
   // TODO: Replace this with smart strchr/strstr, one which ignores
   // characters in quotes of any kind, and permits escaped characters.
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

void kbnode_dump (const kbnode_t *node, FILE *outf, size_t level)
{
#define INDENT(l)    for (size_t i=0; i<((l) * 3); i++) fputc (' ', outf)
   if (!outf)
      outf = stdout;
   if (!node) {
      fprintf (outf, "NULL node object!\n");
      return;
   }

   INDENT (level);
   fprintf (outf, "Node [%s] with parent [%s]: 0x%" PRIx64 "\n",
         node_type_name (node->type),
         node->parent ? kbnode_getvalue_first (node->parent, KBNODE_KEY_ID) : "null",
         node->flags);

   kbsymtab_dump (node->symtab, outf, level);

   size_t nchildren = ds_array_length (node->jobs);
   INDENT (level + 1);
   fprintf (outf, "njobs: %zu\n", nchildren);
   for (size_t i=0; i<nchildren; i++) {
      kbnode_dump ((kbnode_t *)(ds_array_get (node->jobs, i)), outf, level + 1);
   }
   nchildren = ds_array_length (node->jobs);
   INDENT (level + 1);
   fprintf (outf, "nhandlers: %zu\n", nchildren);
   for (size_t i=0; i<nchildren; i++) {
      kbnode_dump ((kbnode_t *)(ds_array_get (node->handlers, i)), outf, level + 1);
   }
#undef INDENT
}

const ds_array_t *kbnode_jobs (const kbnode_t *node)
{
   return node ? node->jobs : NULL;
}

const ds_array_t *kbnode_handlers (const kbnode_t *node)
{
   return node ? node->handlers : NULL;
}

const char **kbnode_keys (const kbnode_t *node)
{
   return kbsymtab_keys (node->symtab);
}

void kbnode_del (kbnode_t *node)
{
   node_del (node);
}

bool kbnode_get_srcdef (const kbnode_t *node, const char **id, const char **fname,
                        size_t *line)
{
   *line = 0;
   *fname = "message not set";
   *id = "id not set";

   if (!node) {
      return false;
   }

   const char **f = kbsymtab_get (node->symtab, KBNODE_KEY_FNAME);
   const char **l = kbsymtab_get (node->symtab, KBNODE_KEY_LINE);
   const char **i = kbsymtab_get (node->symtab, KBNODE_KEY_ID);
   if (!f || !f[0] || !l || !l[0] || !i || !i[0]) {
      return false;
   }

   sscanf (l[0], "%zu", line);
   *fname = f[0];
   *id = i[0];
   return true;
}

const char *kbnode_getvalue_first (const kbnode_t *node, const char *key)
{
   const char *ret = node == NULL ? "" : kbsymtab_get_string (node->symtab, key);
   return ret ? ret : "";
}

const char **kbnode_getvalue_all (const kbnode_t *node, const char *key)
{
   static const char *dummy[] = {
      NULL,
   };

   const char **ret = node == NULL ? dummy : kbsymtab_get (node->symtab, key);
   return ret ? ret : dummy;
}

bool kbnode_set_single (kbnode_t *node, const char *key, size_t index,
                        char *newvalue)
{
   const char **values = kbsymtab_get (node->symtab, key);
   if (!values || !values[0]) {
      return false;
   }
   size_t nvalues = kbutil_strarray_length (values);
   if (index > nvalues) {
      return false;
   }

   free ((void *)values[index]);
   values[index] = newvalue;
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
      KBXERROR ("Failed to open [%s] for reading: %m\n", fname);
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
      // TODO: Replace this with smart strchr/strstr, one which ignores
      // characters in quotes of any kind, and permits escaped characters.
      if ((tmp = strchr (line, '\r'))) {
         KBPARSE_ERROR (fname, lc,
               "Carriage return (\\r) detected on line %zu\n", lc);
         errno = EILSEQ; // TODO: Maybe Windows needs a different error?
         *nerrors = (*nerrors) + 1;
         current = NULL;
         goto cleanup;
      }

      if ((tmp = strchr (line, '\n')) == NULL) {
         KBPARSE_ERROR (fname, lc,
               "Line length limit of %i bytes exceeded\n", line_len);
         INCPTR (*nerrors);
         current = NULL;
         goto cleanup;
      }
      *tmp = 0;

      // TODO: Replace this with smart strchr/strstr, one which ignores
      // characters in quotes of any kind, and permits escaped characters.
      if ((tmp = strchr (line, '#'))) {
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

         // TODO: Replace this with smart strchr/strstr, one which ignores
         // characters in quotes of any kind, and permits escaped characters.
         char *tmp = strchr (line, ']');
         if (!tmp) {
            KBPARSE_ERROR (fname, lc, "Mangled input [%s]\n", line);
            *nerrors = (*nerrors) + 1;
            goto cleanup;
         }
         *tmp = 0;

         if (!(current = node_new (fname, lc, &line[1], NULL, childtype_NONE))) {
            KBPARSE_ERROR (fname, lc,
                  "Node creation attempt failure near: '%s'\n", &line[1]);
            *nerrors = (*nerrors) + 1;
            goto cleanup;
         }

         if (!(ds_array_ins_tail (dst, current))) {
            KBIERROR ("OOM appending new node %s to collection\n", line);
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

      // TODO: Replace this with smart strchr/strstr, one which ignores
      // characters in quotes of any kind, and permits escaped characters.

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

static bool node_filter_handlers (const void *element, void *param)
{
   const kbnode_t *node = element;
   const char **signals = param;

   const char **handled_signals = kbnode_getvalue_all (node, KBNODE_KEY_HANDLES);

   for (size_t i=0; signals && signals[i]; i++) {
      for (size_t j=0; handled_signals[j]; j++) {
         if ((strcmp (signals[i], handled_signals[j])) == 0) {
            return true;
         }
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
      KBIERROR ("OOM error allocating array of %zu entries\n", nelements + 1);
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

static void nc_job (const kbnode_t *node,
                    const char *id, const char *fname, size_t line,
                    size_t *errors, size_t *warnings)
{
   (void)node;
   (void)id;
   (void)fname;
   (void)line;
   (void)errors;
   (void)warnings;
}

static bool period_suffix (const char *src)
{
   // TODO, this should actuall parse and return the entire value
   if (src[1] == 0) {
      return strchr ("smhdwM", src[0]) ? true : false;
   }
   static const char *suffixes[] = {
      "sec", "secs", "second", "seconds",
      "min", "mins", "minute", "minutes",
      "hr", "hrs", "hour", "hours",
      "day", "days",
      "wk", "wks", "week", "weeks",
      "month", "months",
   };

   for (size_t i=0; i<sizeof suffixes/sizeof suffixes[0]; i++) {
      if ((strcmp (src, suffixes[i])) == 0) {
         return true;
      }
   }

   return false;
}

static void nc_periodic (const kbnode_t *node,
                         const char *id, const char *fname, size_t line,
                         size_t *errors, size_t *warnings)
{
   (void)warnings;
   const char **periods = kbsymtab_get (node->symtab, KBNODE_KEY_PERIOD);
   size_t nperiods = kbutil_strarray_length (periods);

   if (nperiods == 0) {
      KBPARSE_ERROR (fname, line,
               "[periodic] node [%s] does not specify any periods\n", id);
      INCPTR (*errors);
      return;
   }

   if (nperiods > 1) {
      KBPARSE_ERROR (fname, line,
               "[periodic] node [%s] has multiple periods. Only one is allowed.\n",
               id);
      INCPTR(*errors);
   }

   const char *tmp = periods[0];
   if (!tmp) {
      KBPARSE_ERROR (fname, line,
               "[periodic] node [%s] has empty value for variable PERIOD\n",
               id);
      INCPTR (*errors);
      return;
   }

   size_t ndigits = 0;
   while ((isdigit (*tmp))) {
      tmp++;
      ndigits++;
   }
   if (!ndigits) {
      KBPARSE_ERROR (fname, line, "node [%s] PERIOD has invalid value [%s]\n",
               id, periods[0]);
      INCPTR (*errors);
   }
   if (!(period_suffix (tmp))) {
      KBPARSE_ERROR (fname, line, "node [%s]: unrecognised  periodic suffix [%s]\n",
               id, tmp);
      INCPTR (*errors);
   }

   // try to parse this, and increment *errors if unable to parse
}

static void nc_entrypoint (const kbnode_t *node,
                           const char *id, const char *fname, size_t line,
                           size_t *errors, size_t *warnings)
{
   (void)node;
   (void)id;
   (void)fname;
   (void)line;
   (void)errors;
   (void)warnings;
}

void kbnode_check (kbnode_t *node, size_t *errors, size_t *warnings)
{
   static const char *required[] = {
      KBNODE_KEY_ID, KBNODE_KEY_MESSAGE,
   };

   // Check for NULL-ness
   if (!node) {
      KBXERROR ("NULL kbnode_t object found!\n");
      INCPTR(*errors);
      return;
   }

   // Ensure that node has fname, id and line number information.
   const char *fname = "fname-missing";
   const char *id = "id-missing";
   size_t line = 0;

   if (!(kbnode_get_srcdef (node, &id, &fname, &line))) {
      KBXERROR ("Node missing id/fname/line: [%s:%s:%zu]\n", id, fname, line);
      INCPTR (*warnings);
   }

   // Check that the node type is one of the valid ones, and for each type
   // perform a type-specific check.
   switch (node->type) {
      case node_type_JOB:
         nc_job (node, id, fname, line, errors, warnings);
         break;

      case node_type_PERIODIC:
         nc_periodic (node, id, fname, line, errors, warnings);
         break;

      case node_type_ENTRYPOINT:
         nc_entrypoint (node, id, fname, line, errors, warnings);
         break;

      case node_type_UNKNOWN:
      default:
         KBPARSE_ERROR (fname, line, "Node [%s] has unknown type: %i/%s\n",
                  id, node->type, node_type_name (node->type));
         INCPTR (*errors);
   }

   // Ensure that the mandatory keys exist
   for (size_t i=0; i<sizeof required/sizeof required[0]; i++) {
      if (!(kbsymtab_exists (node->symtab, required[i]))) {
         KBPARSE_WARN (fname, line, "Node [%s] missing required key '%s'\n",
                  id, required[i]);
         INCPTR (*errors);
      }
   }

   // Possibly refactor this into a different function. For now this is fine
   // as we only have a single set of XOR keys to check
   int exec = kbsymtab_exists (node->symtab, KBNODE_KEY_EXEC) ? 1 : 0;
   int emit = kbsymtab_exists (node->symtab, KBNODE_KEY_EMITS) ? 1 : 0;
   int jobs = kbsymtab_exists (node->symtab, KBNODE_KEY_JOBS) ? 1 : 0;
   if ((exec + emit + jobs) != 1) {
      KBPARSE_ERROR (fname, line,
               "Exactly one of EXEC, EMITS or JOBS must be specified\n");
      INCPTR(*errors);
   }
}

ds_array_t *kbnode_filter_types (const ds_array_t *nodes, const char *type, ...)
{
   va_list ap;
   va_start (ap, type);
   char **types = collect_args (type, ap);
   ds_array_t *ret = ds_array_filter (nodes, node_filter_func_types, types);
   free (types);
   return ret;
}

ds_array_t *kbnode_filter_keyname (const ds_array_t *nodes, const char *keyname, ...)
{
   va_list ap;
   va_start (ap, keyname);
   char **names = collect_args (keyname, ap);
   ds_array_t *ret = ds_array_filter (nodes, node_filter_names, names);
   free (names);
   return ret;
}

ds_array_t *kbnode_filter_handlers (const ds_array_t *nodes, const char **signals)
{
   return ds_array_filter (nodes, node_filter_handlers, signals);
}


kbnode_t *kbnode_instantiate (const kbnode_t *src, ds_array_t *all,
                              size_t *errors, size_t *warnings)
{
   if (!src) {
      KBXERROR ("NULL node found\n");
      INCPTR(*errors);
      return NULL;
   }

   kbnode_t *ret = node_instantiate (src, NULL, childtype_NONE,
                                     all, errors, warnings);
   if (!ret) {
      KBPARSE_ERROR (node_filename (src), node_line (src),
            "Failed to instantiate node\n");
      return NULL;
   }

   return ret;
}

const char **kbnode_resolve (const kbnode_t *node, const char *symbol)
{
   if (!node || !symbol)
      return NULL;

   const char **ret = kbsymtab_get (node->symtab, symbol);

   return ret ? ret : kbnode_resolve (node->parent, symbol);
}


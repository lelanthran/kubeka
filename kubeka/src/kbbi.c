#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>


#include "ds_array.h"
#include "ds_str.h"

#include "kbnode.h"
#include "kbbi.h"
#include "kbutil.h"
#include "kbexec.h"


#define INCPTR(x)    do {\
   (x) = (x) + 1;\
} while (0)


#define KBBI_FUNC(x) \
   static char *x (const char *name, const char *params, const kbnode_t *node,\
                   size_t *nerrors, const char *fname, size_t line)

KBBI_FUNC(bi_setenv);
KBBI_FUNC(bi_getenv);

static const struct {
   const char *name;
   kbbi_fptr_t *fptr;
} functions[] = {
   { "setenv",    bi_setenv },
   { "getenv",    bi_getenv },
};


kbbi_fptr_t *kbbi_fptr (const char *name)
{
   size_t nfuncs = sizeof functions/sizeof functions[0];
   for (size_t i=0; i<nfuncs; i++) {
      if ((strcmp (name, functions[i].name)) == 0) {
         return functions[i].fptr;
      }
   }
   return NULL;
}

static kbnode_t *find_node (const char *node_id, ds_array_t *nodes)
{
   size_t nnodes = ds_array_length (nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_t *node = ds_array_get (nodes, i);
      const char *id = kbnode_getvalue_first (node, KBNODE_KEY_ID);
      if ((strcmp (id, node_id)) == 0) {
         return node;
      }
   }

   return NULL;
}

static bool kbbi_rollback (kbnode_t *node, size_t *nerrors, size_t *nwarnings)
{
   const char *fname = NULL;
   size_t line;
   if (!(kbnode_get_srcdef (node, &fname, &line))) {
      KBERROR ("Node has no source file information");
      INCPTR (*nerrors);
      return false;
   }

   const char **actions = kbnode_getvalue_all (node, KBNODE_KEY_ROLLBACK);
   if (!actions || !actions[0]) {
      INCPTR (*nwarnings);
      KBPARSE_ERROR (fname, line, "No rollback actions found\n");
      return true;
   }

   for (size_t i=0; actions[i]; i++) {
      // TODO: Exec action and capture stdout and return value
   }
   return true;
}

static int kbbi_run (kbnode_t *node, ds_array_t *nodes,
                     size_t *nerrors, size_t *nwarnings)
{
   int ret = EXIT_FAILURE;
   const char *s_id = kbnode_getvalue_first (node, KBNODE_KEY_ID);
   const char *s_message = kbnode_getvalue_first (node, KBNODE_KEY_MESSAGE);
   const char *fname = NULL;
   size_t line = 0;
   const char **s_jobs = kbnode_getvalue_all (node, KBNODE_KEY_JOBS);
   const char *s_exec = kbnode_getvalue_first (node, KBNODE_KEY_EXEC);
   const char *s_emit = kbnode_getvalue_first (node, KBNODE_KEY_EMITS);

   if (!(kbnode_get_srcdef (node, &fname, &line))) {
      KBERROR ("Node has no source file information\n");
      INCPTR (*nerrors);
      goto cleanup;
   }

   if (!s_id || !s_id[0]) {
      KBERROR ("Node %p has no ID field\n", node);
      INCPTR (*nerrors);
      goto cleanup;
   }

   printf ("::STARTING:%s:%s\n", s_id, s_message);

   if (s_emit && s_emit[0]) {
      ds_array_t *handler_nodes = kbnode_filter_handlers (nodes, s_emit, NULL);
      if (!handler_nodes) {
         KBERROR ("OOM allocating handlers list\n");
         goto cleanup;
      }
      size_t nhandler_nodes = ds_array_length (handler_nodes);
      if (!nhandler_nodes) {
         KBPARSE_ERROR (fname, line, "Failed to find any handlers for node\n");
         ds_array_del (handler_nodes);
         goto cleanup;
      }
      ret = 0;
      for (size_t i=0; i<nhandler_nodes; i++) {
         kbnode_t *handler_node = ds_array_get (handler_nodes, i);
         ret += kbbi_run (handler_node, nodes, nerrors, nwarnings);
      }
      ds_array_del (handler_nodes);
      return ret;
   }

   if (s_exec && s_exec[0]) {
      char *result = NULL;
      size_t result_len = 0;
      ret = kbexec_shell (fname, line, s_exec, &result, &result_len);
      printf ("::COMMAND:%s:%zu bytes\n-----\n%s\n-----\n",
               s_exec, result_len, result);
      free (result);
      goto cleanup;
   }

   if (s_jobs && s_jobs[0]) {
      for (size_t i=0; s_jobs[i]; i++) {
         kbnode_t *node = find_node (s_jobs[i], nodes);
         if (!node) {
            INCPTR (*nerrors);
            KBPARSE_ERROR (fname, line, "Cannot find child [%s]\n", s_jobs[i]);
            goto cleanup;
         }
         if ((kbbi_run (node, nodes, nerrors, nwarnings)) != EXIT_SUCCESS) {
            INCPTR (*nwarnings);
            KBPARSE_ERROR (fname, line, "Child failure [%s]: Attempting rollback\n",
                  s_jobs[i]);
            for (size_t j=i; j>0; j--) {
               kbnode_t *node = find_node (s_jobs[j], nodes);
               if (!(kbbi_rollback (node, nerrors, nwarnings))) {
                  INCPTR (*nerrors);
                  KBPARSE_ERROR (fname, line, "Rollback failure in child [%s]\n",
                        s_jobs[j]);
               }
            }
            if (!(kbbi_rollback (find_node (s_jobs[0], nodes),
                                            nerrors, nwarnings))) {
               INCPTR (*nerrors);
               KBPARSE_ERROR (fname, line, "Rollback failure in child [%s]\n",
                     s_jobs[0]);
            }
            goto cleanup;
         }
      }
      ret = EXIT_SUCCESS;
      goto cleanup;
   }

   KBPARSE_ERROR (fname, line,
            "Failed to find one of JOBS[], EXEC or EMITS in node [%s]\n", s_id);

cleanup:
   if (ret != EXIT_SUCCESS) {
      KBPARSE_ERROR (fname, line,
               "Failed to run node [%s]. Full node follows:\n", s_id);
      kbnode_dump (node, stderr);
   }

   return ret;
}

int kbbi_launch (const char *node_id, ds_array_t *nodes,
                 size_t *nerrors, size_t *nwarnings)
{
   kbnode_t *target = find_node (node_id, nodes);
   if (!target) {
      KBERROR ("Node [%s] not found in tree\n", node_id);
      INCPTR (*nerrors);
      return EXIT_FAILURE;
   }

   return kbbi_run (target, nodes, nerrors, nwarnings);
}


#define KBBI_DEFINITION(x) \
   static char *x (const char *name, const char *params, const kbnode_t *node,\
                   size_t *nerrors, const char *fname, size_t line)

KBBI_FUNC(bi_setenv)
{
   (void)name;
   (void)node;

   // Some very platform-specific code goes here, due to how hard it is
   // (some would say impossible) to set environments without leaking
   // memory.
   errno = 0;

   char **p = kbutil_strsplit (params, '=');
   if (!p) {
      KBERROR ("OOM splitting params [%s]\n", params);
      INCPTR (*nerrors);
      return NULL;
   }

   if (!p[0] || !p[1]) {
      KBPARSE_ERROR (fname, line, "Failed to extract NAME=VALUE for setenv()\n"
                                  "[%s] %p %p\n",
                                  params, p[0], p[1]);
      INCPTR (*nerrors);
      kbutil_strarray_del (p);
      return NULL;
   }

   int rc = setenv (p[0], p[1], 1);
   if (rc != 0) {
      KBPARSE_ERROR (fname, line, "Failed to set env [%s] = [%s]\n",
               p[0], p[1]);
      INCPTR (*nerrors);
   }

   kbutil_strarray_del (p);
   return ds_str_dup ("");
}

KBBI_FUNC(bi_getenv)
{
   (void)name;
   (void)node;
   (void)nerrors;
   (void)fname;
   (void)line;

   char *value = getenv (params);
   if (!value) {
      value = "";
   }
   return ds_str_dup (value);
}



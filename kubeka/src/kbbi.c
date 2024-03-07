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

static kbnode_t *find_node (const char *node_id, const ds_array_t *nodes)
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
   const char *id = NULL;
   size_t line;
   if (!(kbnode_get_srcdef (node, &id, &fname, &line))) {
      KBXERROR ("Node has no source file information");
      INCPTR (*nerrors);
      return false;
   }

   const char **actions = kbnode_getvalue_all (node, KBNODE_KEY_ROLLBACK);
   if (!actions || !actions[0] || !actions[0][0]) {
      INCPTR (*nwarnings);
      KBPARSE_ERROR (fname, line, "No rollback actions found for node [%s]\n", id);
      return true;
   }

   KBPARSE_ERROR (fname, line,
         "Attempting ROLLBACK on node [%s] (%zu rollback actions found)\n",
         id, kbutil_strarray_length (actions));
   for (size_t i=0; actions[i]; i++) {
      char *result = NULL;
      size_t result_len = 0;
      int rc = kbexec_shell (fname, line, actions[i], &result, &result_len);
      printf ("::ROLLBACK:%s:%i:%zu bytes\n-----\n%s\n-----\n",
               actions[i], rc, result_len, result);
      if (rc) {
         KBPARSE_ERROR (fname, line, "Node [%s] failed to rollback\n", id);
      } else {
         KBPARSE_ERROR (fname, line,
               "Successful ROLLBACK on node [%s] (%zu rollback actions found)\n",
               id, kbutil_strarray_length (actions));
      }
      free (result);
   }
   return true;
}

static int kbbi_run (kbnode_t *node, size_t *nerrors, size_t *nwarnings)
{
   int ret = EXIT_FAILURE;
   const char *s_message = kbnode_getvalue_first (node, KBNODE_KEY_MESSAGE);
   const char *id = NULL;
   const char *fname = NULL;
   size_t line = 0;
   const char **s_exec = kbnode_getvalue_all (node, KBNODE_KEY_EXEC);
   const char **signals = kbnode_getvalue_all (node, KBNODE_KEY_EMITS);


   bool done = false;

   if (!(kbnode_get_srcdef (node, &id, &fname, &line))) {
      KBXERROR ("Node has no source file information\n");
      INCPTR (*nerrors);
      goto cleanup;
   }

   printf ("::STARTING:%s:%s\n", id, s_message);


   // Execute all the handlers (should this be first?)
   ds_array_t *handler_nodes = kbnode_filter_handlers (kbnode_handlers (node),
                                                       signals);
   size_t nnodes = ds_array_length (handler_nodes);
   if (!handler_nodes) {
      KBIERROR ("OOM allocating handlers list\n");
      INCPTR (*nerrors);
      goto cleanup;
   } else {
      ret = 0;
   }

   for (size_t i=0; i < nnodes; i++) {
      kbnode_t *handler_node = ds_array_get (handler_nodes, i);
      ret += kbbi_run (handler_node, nerrors, nwarnings);
      done = true;
   }
   ds_array_del (handler_nodes);
   if (done) {
      goto cleanup;
   }


   // Execute all the EXEC statements
   for (size_t i=0; s_exec && s_exec[i] && s_exec[i][0]; i++) {
      char *result = NULL;
      size_t result_len = 0;
      ret |= kbexec_shell (fname, line, s_exec[i], &result, &result_len);
      printf ("::COMMAND:%s:%i:%zu bytes\n-----\n%s\n-----\n",
               s_exec[i], ret, result_len, result);
      free (result);
      done = true;
   }
   if (done) {
      goto cleanup;
   }


   // Execute all the jobs
   const ds_array_t *jobs = kbnode_jobs (node);
   nnodes = ds_array_length (jobs);

   for (size_t i=0; i < nnodes; i++) {
      kbnode_t *job = ds_array_get (jobs, i);
      if ((ret = kbbi_run (job, nerrors, nwarnings)) != EXIT_SUCCESS) {
         const char *fname = NULL, *id = NULL;
         size_t line = 0;
         kbnode_get_srcdef (job, &id, &fname, &line);
         KBPARSE_ERROR (fname, line, "Error executing job[%s]:\n", id);
         kbnode_dump (job, stderr, 0);
         INCPTR (*nwarnings);
         for (size_t j=i; j>0; j--) {
            kbnode_t *rbnode = ds_array_get (jobs, j);
            if (!(kbbi_rollback (rbnode, nerrors, nwarnings))) {
               INCPTR (*nerrors);
               KBPARSE_ERROR (fname, line, "Rollback failure in child:\n");
               kbnode_dump (rbnode, stderr, 1);
            }
         }
         if (!(kbbi_rollback (ds_array_get (jobs, 0), nerrors, nwarnings))) {
            INCPTR (*nerrors);
            KBPARSE_ERROR (fname, line, "Rollback failure in child:\n");
            kbnode_dump (ds_array_get (jobs, 0), stderr, 1);
         }
         goto cleanup;
      }
      done = true;
   }
   if (done) {
      goto cleanup;
   }

   KBPARSE_ERROR (fname, line,
            "Failed to find one of JOBS[], EXEC or EMITS in node [%s]\n", id);

cleanup:
   if (ret != EXIT_SUCCESS) {
      KBPARSE_ERROR (fname, line,
               "Failed to run node [%s]. Full node follows:\n", id);
      kbnode_dump (node, stderr, 0);
   }

   return ret;
}

int kbbi_launch (const char *node_id, ds_array_t *nodes,
                 size_t *nerrors, size_t *nwarnings)
{
   kbnode_t *target = find_node (node_id, nodes);
   if (!target) {
      KBXERROR ("Node [%s] not found in tree\n", node_id);
      INCPTR (*nerrors);
      return EXIT_FAILURE;
   }

   return kbbi_run (target, nerrors, nwarnings);
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
      KBIERROR ("OOM splitting params [%s]\n", params);
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



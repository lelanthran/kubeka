
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "ds_array.h"
#include "ds_set.h"
#include "ds_str.h"

#include "kbnode.h"
#include "kbsym.h"
#include "kbtree.h"

#define ERROR(...)      do {\
   fprintf (stderr, "%s:%i Internal error: ", __FILE__, __LINE__);\
   fprintf (stderr, __VA_ARGS__);\
} while (0);

/* ****************************************************************************
 * The following functions together implement a sane c/line argument processing
 * mechanism for both long options and short options, implementing a subset
 * of POSIX and GNU standards for option processing.
 *
 * Long options:
 *    --name | --name= | --name=value
 *
 * Short options ('foo' is the value of option 'z'):
 *    -x -y -z foo | -xyz foo | -xyzfoo
 */

static const char *opt_long (int argc, char **argv, const char *opt)
{
   size_t optlen = strlen (opt);
   for (int i=1; i<argc; i++) {
      if ((memcmp (argv[i], "--", 2)) != 0) {
         continue;
      }
      char *name = &argv[i][2];
      size_t namelen = strlen (name);
      size_t cmpbytes = namelen < optlen ? namelen : optlen;
      if ((memcmp (name, opt, cmpbytes)) != 0) {
         continue;
      }

      char *value = strchr (name, '=');
      if (value) {
         *value++ = 0;
      } else {
         value = "";
      }
      argv[i][0] = 0;
      return value;
   }
   return NULL;
}

static const char *opt_short (int argc, char **argv, char opt)
{
   for (int i=1; i<argc; i++) {
      if (argv[i][0] != '-' || argv[i][1] == '-') {
         continue;
      }
      char *found = strchr (&argv[i][1], opt);
      if (found) {
         *found = '_';
         if (found[1] == 0) {
            return argv[i + 1] ? argv[i+1] : "";
         } else {
            *found = 0;
            return &found[1];
         }
      }
   }
   return NULL;
}

static size_t opt_unrecognised (int argc, char **argv)
{
   size_t nerrors = 0;
   for (int i=0; i<argc; i++) {
      if ((memcmp (argv[i], "--", 2)) == 0) {
         fprintf (stderr, "Unrecognised option [%s]\n", argv[i]);
         nerrors++;
         continue;
      }
      if (argv[i][0] == '-') {
         for (size_t j=1; argv[i][j]; j++) {
            if (argv[i][j] != '_') {
               fprintf (stderr, "Unrecognised option [-%c]\n", argv[i][j]);
               nerrors++;
            }
         }
      }
   }
   return nerrors;
}

static bool opt_bool (int argc, char **argv, const char *lopt, char sopt)
{
   const char *opt = opt_long (argc, argv, lopt);
   return opt_short (argc, argv, sopt) || opt;
}

static void print_help_msg (void)
{
   static const char *msg[] = {
"Kubeka - a Continuous Deployment service",
"",
"SYNOPSIS",
"  kubeka [-h | --help]",
"  kubeka [-d | --daemonize] [-p | --path] [-W | -Werror] [-f | --file=<filename>]",
"",
"DESCRIPTION",
"  Kubeka (meaning 'put') is a simple tool to automate continuous deployment. On",
"  startup all *.kubeka configuration files are loaded and then linted for errors.",
"  Thereafter kubeka will wait for one of multiple events (as specified in the ",
"  configuration files) and proceed to execute all dependent jobs as specified in",
"  the configuration files.",
"",
"  The configuration files are described in the official Kubeka help documents. See",
"  https://github.com/lelanthran/kubeka",
"",
"OPTIONS",
"  -h | --help",
"              Display this message and exit with a successful return code.",
"  -d | --daemon",
"              Run in the background, and return a successful return code.",
"  -p | --path",
"              Path containing additional configuration *.kubeka files. This",
"              option can be specified multiple times, once for each path that",
"              must be searched for *.kubeka files.",
"  -f | --file=<filename>",
"              An additional *.kubeka file. This option can be specified multiple",
"              times, once for each additional file that must be parsed.",
"  -W | --Werror",
"              Treat all warnings as errors.",
"",
"",
   };

   for (size_t i=0; i<sizeof msg/sizeof msg[0]; i++) {
      printf ("%s\n", msg[i]);
   }
}

static void process_path (ds_array_t *files, const char *path)
{
   static const char *ext = ".kubeka";
   const size_t extlen = strlen (ext);

   errno = 0;
   DIR *dirp = opendir (path);
   if (errno == ENOTDIR) {
      if (!(ds_array_ins_tail (files, ds_str_dup (path)))) {
         ERROR ("OOM storing file [%s]\n", path);
      }
      return;
   }

   if (!dirp) {
      fprintf (stderr, "Failed to open directory [%s] for reading: %m\n", path);
      return;
   }
   struct dirent *de;
   while ((de = readdir (dirp))) {
      if (de->d_name[0] == '.') {
         continue;
      }
      if (de->d_type == DT_DIR) {
         char *tmp = ds_str_cat (path, "/", de->d_name, NULL);
         process_path (files, tmp);
         free (tmp);
      }
      size_t namelen = strlen (de->d_name);
      if (namelen <= extlen) {
         continue;
      }
      if ((memcmp (&de->d_name[namelen - extlen], ext, extlen)) == 0) {
         if (!(ds_array_ins_tail (files, ds_str_cat (path, "/", de->d_name, NULL)))) {
            ERROR ("OOM storing file %s/[%s]\n", path, de->d_name);
         }
      }
   }
   closedir (dirp);
}

static ds_array_t *process_paths (ds_array_t *dirs)
{
   ds_array_t *files = ds_array_new ();
   if (!files) {
      ERROR ("OOM creating fileslist\n");
      return NULL;
   }

   size_t ndirs = ds_array_length (dirs);
   for (size_t i=0; i<ndirs; i++) {
      process_path (files, (char *)ds_array_get (dirs, i));
   }
   return files;
}


int main (int argc, char **argv)
{
   int ret = EXIT_FAILURE;
   ds_array_t *paths = NULL;
   ds_array_t *files = NULL;
   ds_array_t *nodes = NULL;
   ds_array_t *dedup_nodes = NULL;
   ds_array_t *entrypoints = NULL;
   ds_array_t *trees = NULL;


   /* ***********************************************************************
    * 1. Read all options
    * ***********************************************************************/
   // 1.1 Help?
   if ((opt_bool (argc, argv, "help", 'h'))) {
      print_help_msg ();
      return EXIT_SUCCESS;
   }

   // 1.2 Read all the -p/--path options
   paths = ds_array_new ();
   if (!paths) {
      ERROR ("OOM allocating paths array\n");
      goto cleanup;
   }
   if (!(ds_array_ins_tail (paths, ds_str_dup ("/etc/kubeka")))) {
      ERROR ("OOM storing first path element\n");
      goto cleanup;
   }

   const char *opt_path = NULL;
   size_t counter = 0;
   while ((opt_path = opt_long (argc, argv, "path"))) {
      if (!(ds_array_ins_tail (paths, (void *)ds_str_dup (opt_path)))) {
         ERROR ("OOM storing --path=%s (%zu)\n", opt_path, counter);
         goto cleanup;
      }
      counter++;
   }
   counter = 0;
   while ((opt_path = opt_short (argc, argv, 'p'))) {
      if (!(ds_array_ins_tail (paths, (void *)ds_str_dup (opt_path)))) {
         ERROR ("OOM storing -p=%s (%zu)\n", opt_path, counter);
         goto cleanup;
      }
      counter++;
   }

   // 1.3 Read all -f/--file options
   const char *opt_file = NULL;
   while ((opt_file = opt_long (argc, argv, "file"))) {
      if (!(ds_array_ins_tail (paths, (void *)ds_str_dup (opt_path)))) {
         ERROR ("OOM storing --file=%s (%zu)\n", opt_path, counter);
         goto cleanup;
      }
      counter++;
   }
   counter = 0;
   while ((opt_path = opt_short (argc, argv, 'f'))) {
      if (!(ds_array_ins_tail (paths, (void *)ds_str_dup (opt_path)))) {
         ERROR ("OOM storing -f=%s (%zu)\n", opt_path, counter);
         goto cleanup;
      }
      counter++;
   }

   // 1.4 Check if we are to daemonize
   if ((opt_bool (argc, argv, "daemon", 'd'))) {
      pid_t pid = fork ();
      if (pid < 0) {
         ERROR ("Failed to create child process: %m\n");
         goto cleanup;
      }
      if (pid > 0) {
         printf ("Detached process created with PID: %i\n", pid);
         ret = EXIT_SUCCESS;
         goto cleanup;
      }
      printf ("Daemon %s started with pid %i\n", argv[0], pid);
   }

   // 1.4 Set the remaining options
   bool opt_werror = opt_bool (argc, argv, "Werror", 'W');

   // At this point we have completed all option processing, may as well check if any
   // unrecognised options were specified and exit with a message if so.
   size_t nbadopts = opt_unrecognised (argc, argv);
   if (nbadopts) {
      fprintf (stderr, "Found %zu unrecognised options, aborting.\n", nbadopts);
      goto cleanup;
   }



   /* ***********************************************************************
    * 2. Read directories recursively for *.kubeka files, both system dirs as
    *  well as user-specified dirs
    * ***********************************************************************/

   // 2.1. Generate a list of files to read and load
   if (!(files = process_paths (paths))) {
      ERROR ("Fatal error  processing  paths\n");
      goto cleanup;
   }

   printf ("Processing %zu kubeka files\n", ds_array_length (files));



   /* ***********************************************************************
    * 3. Create single giant list of nodes, duplicates included!
    * ***********************************************************************/
   if (!(nodes = ds_array_new ())) {
      ERROR ("Failed to create array to store nodes\n");
      goto cleanup;
   }
   size_t nfiles = ds_array_length (files);
   size_t nerrors = 0, nwarnings = 0;
   size_t warnings = 0, errors = 0;
   for (size_t i=0; i<nfiles; i++) {
      const char *file = ds_array_get (files, i);
      printf ("Reading %s ...\n", file);
      warnings = 0;
      errors = 0;
      if (!(kbnode_read_file (nodes, file, &errors, &warnings))) {
         if (errors) {
            fprintf (stderr, "Fatal errors while parsing [%s], aborting\n", file);
         }
         if (warnings) {
            fprintf (stderr, "Encountered warnings while parsing [%s]\n", file);
         }
         nerrors += errors;
         nwarnings += warnings;
      }
   }



   /* ***********************************************************************
    * 4. Gather all nodes into a deduplicated collection, discarding (and
    *    warning about) duplicate nodes.
    * ***********************************************************************/
   size_t ndups = 0;
   warnings = 0;
   errors = 0;
   if (!(dedup_nodes = kbtree_coalesce (nodes, &ndups, &errors, &warnings))) {
      ERROR ("Internal error, aborting\n");
      goto cleanup;
   }

   printf ("Checking for duplicates ... ");
   if (ndups) {
      printf ("Found %zu duplicate%s in node list\n", ndups, ndups ? "" : "s");
      nerrors += ndups;
   } else {
      printf ("none\n");
   }
   nwarnings += warnings;
   nerrors += errors;


   /* ***********************************************************************
    * 5. Perform a basic sanity check on every node:
    *    1. Conflicting variables?
    *    2. Missing mandatory/required variables?
    * ***********************************************************************/
   size_t nnodes = ds_array_length (dedup_nodes);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_t *node = ds_array_get (dedup_nodes, i);
      kbnode_check (node, &nerrors, &nwarnings);
   }


   /* ***********************************************************************
    * 6. Instantiate all the entrypoints. This doesn't run them, though, it
    * simply creates a runtime tree with each entrypoint as the root node, to
    * be evaluated at a later time.
    * ***********************************************************************/
   if (!(trees = ds_array_new ())) {
      ERROR ("OOM creating root node arrays\n");
      nerrors++;
      goto cleanup;
   }

   entrypoints = kbnode_filter_types (dedup_nodes, KBNODE_TYPE_PERIODIC,
                                                   KBNODE_TYPE_ENTRYPOINT,
                                                   NULL);

   nnodes = ds_array_length (entrypoints);
   printf ("Found %zu entrypoint nodes\n", nnodes);
   for (size_t i=0; i<nnodes; i++) {
      const kbnode_t *ep = ds_array_get (entrypoints, i);
      kbnode_t *newnode = kbnode_instantiate (ep, dedup_nodes, &nerrors, &nwarnings);
      if (!newnode) {
         ERROR ("Errors encountered during instantiation of node\n");
      }
      if (!(ds_array_ins_tail (trees, newnode))) {
         ERROR ("OOM storing new root node\n");
      }
   }



   printf ("Found %zu errors and %zu warnings\n", nerrors, nwarnings);
   printf ("Found %zu nodes\n", ds_array_length (nodes));


   /* ***********************************************************************
    * X. Decide whether to continue or not. Errors unconditionally cause
    *    a jump to the exit. Warnings only cause a jump if the user specified
    *    -W/--Werror
    * ***********************************************************************/
   // Finally, print out error count and warning count, and determine if we
   // must abort execution.
   if (nerrors) {
      fprintf (stderr, "Aborting due to %zu error%s\n",
            nerrors, nerrors == 1 ? "" : "s");
      goto cleanup;
   }
   if (nwarnings && opt_werror) {
      fprintf (stderr, "Aborting due to %zu warning%s (--Werror specified)\n",
            nwarnings, nwarnings == 1 ? "" : "s");
      goto cleanup;
   }



   // 7. Start a thread for each entrypoint.
   ret = EXIT_SUCCESS;
cleanup:
   fprintf (stdout, "***** Return value: %i *****\n", ret);
   ds_array_iterate (dedup_nodes, (void (*) (void *, void*))kbnode_dump, stdout);
   ds_array_fptr (paths, free);
   ds_array_fptr (files, free);
   ds_array_fptr (nodes, (void (*) (void *))kbnode_del);
   ds_array_del (paths);
   ds_array_del (files);
   ds_array_del (nodes);

   ds_array_del (dedup_nodes);
   ds_array_del (entrypoints);
   ds_array_del (trees);
   return ret;
}

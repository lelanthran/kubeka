
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "ds_array.h"
#include "ds_set.h"
#include "ds_str.h"

#include "kbnode.h"
#include "kbsym.h"
#include "kbtree.h"
#include "kbbi.h"

#define ERROR(...)      do {\
   fprintf (stderr, "%s:%i Internal error: ", __FILE__, __LINE__);\
   fprintf (stderr, __VA_ARGS__);\
} while (0);

static sig_atomic_t g_endloop = 0;
static void sigh (int n)
{
   if (n == SIGINT) {
      g_endloop = 1;
   }
}

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
"  Thereafter kubeka will wait for one of multiple events (specified in the ",
"  configuration files) and proceed to execute all dependent jobs, depending on",
"  the trigger for starting a job, as specified in the the configuration files.",
"",
"  The configuration files are described in the official Kubeka help documents. See",
"  https://github.com/lelanthran/kubeka",
"",
"OPTIONS",
"  -h | --help",
"              Display this message and exit with a successful return code.",
"  -d | --daemon",
"              Run in the background, and return a successful return code. Note",
"              that exactly ONE OF `--daemon` or `--job` MUST BE SPECIFIED.",
"  -l | --lint",
"              Read and parse all files for errors and warnings. Do not attempt",
"              to execute any node. Any `--daemon` or `--job` flags are ignored.",
"  -p | --path",
"              Path containing additional configuration *.kubeka files. This",
"              option can be specified multiple times, once for each path that",
"              must be searched for *.kubeka files.",
"  -f | --file=<filename>",
"              An additional *.kubeka file. This option can be specified multiple",
"              times, once for each additional file that must be parsed.",
"  -j | --job=<job-id>",
"              The single job to execute. This is to allow execution of jobs from",
"              the command-line. The job has to be of type `entrypoint`. Note that",
"              exactly ONE OF `--daemon` or `--job` MUST BE SPECIFIED.",
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

#if 0
static void dumpnode (const void *param_node, void *param_file)
{
   const kbnode_t *node = param_node;
   FILE *outf = param_file;
   kbnode_dump (node, outf, 0);
}
#endif

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

   // 1.4.1 Check if running in single-shot mode.
   const char *opt_entry = opt_short (argc, argv, 'j');
   if (!opt_entry) {
      opt_entry = opt_long (argc, argv, "job");
   }
   // 1.4.2 Only a single -j flag is supported in single-shot mode
   if (opt_short (argc, argv, 'j') || opt_long (argc, argv, "job")) {
      ERROR ("Cannot specify more than one job to run\n");
      goto cleanup;
   }

   // 1.5 Check if we are to lint only
   bool opt_lint = opt_bool (argc, argv, "lint", 'l');

   // 1.6 Check if we are to daemonize
   bool opt_daemon = opt_bool (argc, argv, "daemon", 'd');

   // Sanity check - user cannot specify both jobs and daemonize
   if (opt_entry && opt_daemon) {
      ERROR ("Cannot specify both a job to run and --daemonize\n");
      goto cleanup;
   }
   // Sanity check - user MUST specify ONE OF lint, daemonize or job
   if (!opt_daemon && !opt_lint && !opt_entry) {
      ERROR ("Must specify one of --daemon, --lint or --job\n");
      goto cleanup;
   }


   if (opt_daemon) {
      if (!opt_lint) {
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
   }

   // 1.7 Set the remaining options
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

   printf ("Checking for duplicates ... ");
   if (!(dedup_nodes = kbtree_coalesce (nodes, &ndups, &errors, &warnings))) {
      ERROR ("Internal error, aborting\n");
      goto cleanup;
   }

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



   /* ***********************************************************************
    * 7. Evaluate all the entrypoints. This *still** doesn't run them, though.
    * The evaluation attempts to resolve every symbol only.
    * ***********************************************************************/



   nnodes = ds_array_length (trees);
   for (size_t i=0; i<nnodes; i++) {
      kbnode_t *root = ds_array_get (trees, i);
      size_t errors = 0,
             warnings = 0;

      kbtree_eval (root, &errors, &warnings);
      printf ("Node [%s]: %zu errors, %zu warnings\n",
               kbnode_getvalue_first (root, KBNODE_KEY_ID), errors, warnings);
      nerrors += errors;
      nwarnings += warnings;
   }


   /* ***********************************************************************
    * 8. Decide whether to continue or not. Errors unconditionally cause
    *    a jump to the exit. Warnings only cause a jump if the user specified
    *    -W/--Werror
    * ***********************************************************************/



   printf ("Linting complete.\n");
   printf ("Found %zu errors and %zu warnings\n", nerrors, nwarnings);
   printf ("Found %zu nodes (%zu runnable)\n",
            ds_array_length (nodes), ds_array_length (trees));

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


   /* ***********************************************************************
    * 9. If user just wants to lint, we exit now.
    *
    */
   if (opt_lint) {
      ret = EXIT_SUCCESS;
      goto cleanup;
   }



   /* ***********************************************************************
    * 10. Finally, run all the entrypoints. For a daemon, we create a new thread
    * for each entrypoint, which waits until it is triggered. For a command-line
    * invocation, we sequentially step through the entrypoints in `tree` and
    * execute each one in turn.
    * ***********************************************************************/


   // If an entrypoint is specified, run it then exit.
   if (opt_entry) {
      // Set ret depending on what the execution of that job resulted in
      ret = kbbi_launch (opt_entry, trees, &nerrors, &nwarnings);
      if (ret != EXIT_SUCCESS) {
         fprintf (stderr, "Failed to execute job [%s]: %zu errors, %zu warnings\n",
               opt_entry, nerrors, nwarnings);
      }

      goto cleanup;
   }

   // If no entrypoint specified, start a thread for each entrypoint
   if (!opt_entry) {
      // Wait for SIGINT to tell us to exit
      if ((signal (SIGINT, sigh) == SIG_ERR)) {
         ERROR ("Failed to set signal handler for SIGINT: %m\n");
         goto cleanup;
      }

      // For each entrypoint in 'trees':
      // 1. If it is of type 'periodic', start a thread with the period
      //    set.
      // 2. ... More thread types go here ...
      while (!g_endloop) {
         // Do nothing
      }

      // Tell all threads to end

      // Wait for all threads to end
      ret = EXIT_SUCCESS;
   }



   // TODO: This must come out, not needed when everything is working
   ret = EXIT_SUCCESS;
cleanup:
   // ds_array_iterate (trees, (void (*) (void *, void*))kbnode_dump, stdout);
   ds_array_fptr (paths, free);
   ds_array_fptr (files, free);
   ds_array_fptr (nodes, (void (*) (void *))kbnode_del);
   ds_array_del (paths);
   ds_array_del (files);
   ds_array_del (nodes);

   ds_array_del (dedup_nodes);
   ds_array_del (entrypoints);
   ds_array_fptr (trees, (void (*) (void *))kbnode_del);
   ds_array_del (trees);
   if (ret != EXIT_SUCCESS) {
      ret = EXIT_FAILURE;
   }
   printf ("::EXITCODE:%i\n", ret);
   return ret;
}


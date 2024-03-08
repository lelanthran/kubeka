#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

#include "ds_array.h"
#include "ds_str.h"

#include "kbnode.h"
#include "kbexec.h"
#include "kbutil.h"

bool resize_buffer (char **dst, size_t *len, size_t index)
{
   index += 2;
   if (index < *len) {
      return true;
   }

   size_t newsize = index * 2;
   char *tmp = realloc (*dst, newsize + 2);
   if (!tmp) {
      return false;
   }

   *dst = tmp;
   *len = newsize; // exclude +2; better to reallocate earlier than necessary
   return true;
}

static bool spawn_child (int *fds,
                         const char *id, const char *fname, size_t line,
                         const char *wdir, uid_t uid,
                         const char *command)
{
   FILE *inf = NULL;
   int rc = 0;

   close (fds[0]);

   // Drop privileges
   if (uid != (uid_t)-1) {
      // Drop privileges
      if ((setuid (uid)) != 0) {
         KBPARSE_ERROR (fname, line, "Node [%si]: failed userswitch [%i]: %m\n",
                  id, (int)uid);
         return false;
      }
   }

   // Switch directory
   if ((chdir (wdir)) != 0) {
      KBPARSE_ERROR (fname, line, "Node [%s]: failed to switch to [%s]: %m\n",
               id, wdir);
      return false;
   }

   // Spawn the command
   errno = 0;
   if ((inf = popen (command, "r")) == NULL) {
      KBPARSE_ERROR (fname, line, "Node [%s]: failed to execute command [%s]: %m\n",
               id, command);
      return false;
   }

   // Read output from process, write to fd
   while (!feof (inf) && !ferror (inf)) {
      uint8_t buf[32];
      size_t nbytes = fread (buf, 1, sizeof buf, inf);
      write (fds[1], buf, nbytes);
   }

   if (inf) {
      rc = pclose (inf);
   }
   static const uint8_t delim = 0;
   write (fds[1], &delim, 1);
   write (fds[1], &rc, sizeof rc);

   close (fds[1]);

   exit (rc);
}

#if 0
void dumphex (const void *data, size_t len)
{
   const uint8_t *buf = data;
   printf ("---\n");
   while (len--) {
      printf ("0x%02x-", *buf++);
   }
   printf ("\n---");
}
#endif

int kbexec_shell (const kbnode_t *node, const char *command,
                  char **dst, size_t *dstlen)
{
   /* ************************************************************************
    * Highly inefficient to perform a double spawn, but it allows the process
    * to be isolated correctly.
    *
    * 1. The node's relevant information is retrieved (command,
    *    working directory, target user to execute as, etc).
    * 2. The working directory is created, and ownership of the directory
    *    is transferred to the target user.
    * 3. The pipe is created, to feed data back to parent.
    * 4. A fork() is performed:
    *       -  Child sets the current working directory and drops privileges to
    *          the target user.
    *       -  Parent reads and stores data from pipe until EOF
    * 5. Child performs a `popen`, writing all data from the process to the pipe,
    *    until EOF.
    * 6. Child writes the exit code as an int to the pipe before closing it
    *    with a nul byte preceding the exit code.
    * 7. Child `exits()` with the return code of the spawned process.
    * 8. Parent gets EOF on pipe, stops reading.
    * 9. Parent parent finds the terminating nul in the string, and reads the
    *    int exit code after it.
    * 10. Parent returns the exit code (buffer is owned by caller, so nothing to
    *    return there).
    */

   int ret = EXIT_FAILURE;

   const char *id = "Not set", *fname = "Not set";
   size_t line = 0;

   char *wdir = NULL;
   char *wuser = NULL;
   char *wgroup = NULL;
   uid_t pw_uid = (uid_t)-1;
   gid_t pw_gid = (gid_t)-1;

   int fds[2] = { -1, -1 };
   int rc = -1;

   /* ********************************************************************
    * Get the node information
    */

   if (!(kbnode_get_srcdef (node, &id, &fname, &line))) {
      KBXERROR ("Failed to get node information\n");
      return EXIT_FAILURE;
   }

   wdir = ds_str_dup (kbnode_getvalue_first (node, KBNODE_KEY_WDIR));
   wuser = ds_str_dup (kbnode_getvalue_first (node, KBNODE_KEY_WUSER));


   /* ********************************************************************
    * Create the working environment.
    */

   if (wuser && wuser[0]) {
      // Get target user information
      struct passwd *pwrec = getpwnam (wuser);
      if (!pwrec) {
         KBPARSE_ERROR (fname, line, "Failed to lookup user [%s]: %m\n", wuser);
         goto cleanup;
      }
      pw_uid = pwrec->pw_gid;
      pw_gid = pwrec->pw_gid;
   }

   if (!wdir || !wdir[0]) {
      free (wdir);
      // No directory specified, make a temporary directory
      if (!(wdir = ds_str_cat ("/tmp/node-", id, "XXXXXX", NULL))) {
         KBPARSE_ERROR (fname, line, "OOM creating name node-%s-XXXXXX\n", id);
         goto cleanup;
      }
      if (!(mkdtemp (wdir))) {
         KBPARSE_ERROR (fname, line, "Failure creating dir %s: %m\n", wdir);
         goto cleanup;
      }
      // Change user/group ownership. The manpage specifies that user
      // ownership remains unchanged if specified as (uid_t)-1 and
      // group ownership remains unchanged if specified as (gid_t)-1.
      if ((chown (wdir, pw_uid, pw_gid)) != 0) {
         KBPARSE_ERROR (fname, line,
                  "Failed to change ownership of [%s] to [%s]: %m\n", wdir, wuser);
         goto cleanup;
      }
   }

   // Create pipe
   if ((pipe (fds)) != 0) {
      KBPARSE_ERROR (fname, line, "Failed to create pipe: %m\n");
      goto cleanup;
   }

   rc = fork ();
   if (rc < 0) {
      KBPARSE_ERROR (fname, line, "Failed to create new process: %m\n");
      goto cleanup;
   }

   size_t index = 0;
   if (rc != 0) { // Parent
      close(fds[1]);
      fds[1] = -1;
      *dstlen = 0;
      if (!(resize_buffer (dst, dstlen, index))) {
         KBIERROR ("Failed to resize array");
         goto cleanup;
      }
      (*dst)[0] = 0;

      uint8_t buf[32];
      ssize_t nbytes = 0;
      while ((nbytes = read (fds[0], buf, sizeof buf)) > 0) {
         if (!(resize_buffer (dst, dstlen, index + nbytes + 1))) {
            KBIERROR ("Failed to resize array");
            goto cleanup;
         }
         memcpy (&(*dst)[index], buf, nbytes);
         index += nbytes;
         (*dst)[index] = 0;
      }
      close (fds[0]);
      fds[0] = -1;
      index -= sizeof ret;
      memcpy (&ret, &(*dst)[index], sizeof ret);
      *dstlen = index - 1;
   } else { // Child
      if (!(spawn_child (fds, id, fname, line, wdir, pw_uid, command))) {
         KBPARSE_ERROR (fname, line, "Failed to spawn isolation process: %m\n");
         goto cleanup;
      }
      ret = EXIT_SUCCESS;
   }

cleanup:
   if (wdir) {
      // TODO: Perform rm -rf on the directory. Maybe just use the damn shell.
      char *tmp = ds_str_cat ("rm -rf ", wdir, NULL);
      if (!tmp) {
         KBPARSE_ERROR (fname, line, "Node [%s]: failed to remove tempdir %s\n",
                  id, wdir);
         KBPARSE_ERROR (fname, line, "Remove dir %s manually\n", wdir);
      } else {
         errno = 0;
         if ((system (tmp)) != 0) {
            KBPARSE_ERROR (fname, line, "Failed to execute [%s]: %m\n", tmp);
         }
      }
      free (tmp);
   }

   free (wdir);
   free (wuser);
   free (wgroup);

   if (fds[0] > 0) {
      close (fds[0]);
   }
   if (fds[1] > 0) {
      close (fds[1]);
   }

   return ret;
}


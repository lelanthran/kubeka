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

#define WARN(...)     do {\
   fprintf (stderr, "[%s:%i] Program warning: ", __FILE__, __LINE__);\
   fprintf (stderr, __VA_ARGS__);\
} while (0)

#define ERROR(...)     do {\
   fprintf (stderr, "[%s:%i] Program error: ", __FILE__, __LINE__);\
   fprintf (stderr, __VA_ARGS__);\
} while (0)

#define PARSE_ERROR(fname,line, ...)     do {\
   fprintf (stderr, "Error :%s+%zu: ", fname, line);\
   fprintf (stderr, __VA_ARGS__);\
} while (0)



/* ***********************************************************
 * Misc utility functions
 */
void strarray_del (char **sa)
{
   for (size_t i=0; sa && sa[i]; i++) {
      free (sa[i]);
   }
   free (sa);
}

char *strarray_format (char **sa)
{
   char *ret = ds_str_dup ("[ ");
   const char *delim = "";
   for (size_t i=0; sa && sa[i]; i++) {
      if (!(ds_str_append (&ret, delim, sa[i], " ", NULL))) {
         ERROR ("OOM error formatting array\n");
         free (ret);
         return NULL;
      }
      delim = ",";
   }
   if (!(ds_str_append (&ret, "]", NULL))) {
      free (ret);
      return NULL;
   }

   return ret;
}

size_t strarray_length (char **sa)
{
   size_t ret = 0;
   if (!sa || !sa[0])
      return 0;

   for (size_t i=0; sa[i]; i++) {
      ret++;
   }
   return ret;
}

char **strarray_append (char ***dst, char *s)
{
   if (!dst || !s) {
      return NULL;
   }

   size_t n = strarray_length (*dst);
   char **tmp = realloc (*dst, (sizeof *dst) * (n + 2));
   if (!tmp) {
      return NULL;
   }

   tmp [n] = s;
   tmp [n + 1] = NULL;
   *dst = tmp;
   return *dst;
}


static char **parse_value (char *value)
{
   char **ret = NULL;
   if (value[0] != '[') {
      if (!(ret = calloc (2, sizeof *ret))) {
         return false;
      }
      ret[0] = ds_str_dup (value);
      return ret;
   }

   value = &value[1];
   // TODO: All occurrences of `strchr()` must be replaced with
   // mystrchr which returns the first non-escaped character.
   char *tmp = strchr (value, ']');
   if (!tmp) {
      return NULL;
   }
   *tmp = 0;

   size_t count = 0;
   static const char *delims = ",";
   char *tok = NULL;
   char *saveptr = NULL;

   char *copy = ds_str_dup (value);
   if (!copy) {
      return NULL;
   }

   tmp = copy;
   while ((tok = strtok_r (tmp, delims, &saveptr))) {
      tmp = NULL;
      count++;
   }
   free (copy);

   if (!(ret = calloc (count + 1, sizeof *ret))) {
      return NULL;
   }

   tmp = value;
   saveptr = NULL;
   size_t i = 0;
   while ((tok = strtok_r (tmp, delims, &saveptr))) {
      tmp = NULL;
      ret[i++] = ds_str_trim (ds_str_dup (tok));
   }
   return ret;
}


/* ***********************************************************
 * Symbol table datastructure
 */
struct symtab_t {
   ds_hmap_t *table; // { char *: char ** }
};

static void symtab_dump (const struct symtab_t *s, FILE *outf)
{
   if (!outf)
      outf = stdout;

   if (!s) {
      fprintf (outf, "NULL symtab_t object\n");
      return;
   }

   char **keys = NULL;
   size_t nkeys = ds_hmap_keys (s->table, (void ***)&keys, NULL);
   if (nkeys == (size_t)-1) {
      WARN ("Failed to retrieve elements of symbol table\n");
      return;
   }

   char *tmp = NULL;

   for (size_t i=0; i<nkeys && keys && keys[i]; i++) {
      char **value = NULL;
      if (!(ds_hmap_get_str_ptr(s->table, keys[i], (void **)&value, NULL))) {
         WARN ("Failed to retrieve value for key [%s]\n", keys[i]);
      }
      free (tmp);
      if (!(tmp = strarray_format (value))) {
         ERROR ("OOM trying to format array [%s]\n", keys[i]);
         goto cleanup;
      }
      fprintf (outf, "   %s: %s\n", keys[i], tmp);
   }

cleanup:
   free (tmp);
   free (keys);
}


static bool symtab_init (struct symtab_t *dst)
{
   return (dst && (dst->table = ds_hmap_new (512))) ? true : false;
}

static void symtab_clear (struct symtab_t *st)
{
   char **keys = NULL;
   size_t nkeys = 0;
   nkeys = ds_hmap_keys (st->table, (void ***)&keys, NULL);

   for (size_t i=0; i<nkeys && keys && keys[i]; i++) {
      char **values = NULL;
      if (!(ds_hmap_get_str_ptr (st->table, keys[i], (void **)&values, NULL))) {
         WARN ("Failed to get known good key [%s]\n", keys[i]);
      }
      strarray_del (values);
   }
   free (keys);
   ds_hmap_del (st->table);
}

static char **symtab_get (struct symtab_t *st, const char *key)
{
   if (!st)
      return NULL;

   char **ret = NULL;
   if (!(ds_hmap_get_str_ptr (st->table, key, (void **)&ret, NULL))) {
      WARN ("Failed to find symbol [%s]\n", key);
   }
   return ret;
}

enum keytype_t {
   keytype_ERROR = 0,
   keytype_INDEX,    // specific indexed element: key, key[4], etc
   keytype_ARRAY,    // All elements of the array, as an array: key[]
   keytype_COUNT,    // Number of elements in the array: key[#]
   keytype_CONCAT,   // All elements in array, concatenated together: key[*]
   keytype_FORMAT,   // All elements in array, in array format: key[@]
};

static enum keytype_t detect_keytype (const char *key, size_t *value)
{
   enum keytype_t ret = keytype_ERROR;

   char *copy = ds_str_dup (key);
   if (!copy) {
      return keytype_ERROR;
   }

   char *start = strchr (copy, '[');
   if (!start) {
      ret = keytype_INDEX;
      *value = 0;
      goto cleanup;
   }
   *start++ = 0;

   char *end = strchr (start, ']');
   if (!end) { // got a '[' without a ']'
      goto cleanup;
   }

   if (*start == ']') {
      ret = keytype_ARRAY;
      goto cleanup;
   }
   if (*start == '#') {
      ret = keytype_COUNT;
      goto cleanup;
   }
   if (*start == '*') {
      ret = keytype_CONCAT;
      goto cleanup;
   }
   if (*start == '@') {
      ret = keytype_FORMAT;
      goto cleanup;
   }

   if (isdigit (*start)) {
      *end = 0;
      if ((sscanf (start, "%zu", value)) != 1) {
         goto cleanup;
      }
      ret = keytype_INDEX;
   }

cleanup:
   free (copy);
   return ret;
}

bool match_any (int value, int sentinel, int match, ...)
{
   va_list ap;
   va_start (ap, match);
   while (match != sentinel) {
      if (value == match) {
         return true;
      }
      match = va_arg (ap, int);
   }

   va_end (ap);
   return false;
}

static bool symtab_set (const char *fname, size_t lc,
                        struct symtab_t *st, const char *key, char *value)
{
   bool error = true;
   char **varray = NULL;
   char **existing = NULL;
   size_t index = (size_t)-1;
   char *keycopy = NULL;

   // Normalise the key (remove the `[]`)
   if (!(keycopy = ds_str_dup (key))) {
      PARSE_ERROR (fname, lc, "OOM Error copying key\n");
      goto cleanup;
   }
   char *tmp = strchr (keycopy, '[');
   if (tmp) {
      *tmp = 0;
   }

   // Determine what type of key this is
   enum keytype_t keytype = detect_keytype (keycopy, &index);
   if (keytype == keytype_ERROR) {
      PARSE_ERROR (fname, lc, "Unrecognised key syntax: `%s`\n", value);
      goto cleanup;
   }

   // Split the value into an array of values
   if (!(varray = parse_value (value))) {
      PARSE_ERROR (fname, lc, "OOM trying to parse '%s'\n", value);
      goto cleanup;
   }

   // Get the existing values, if any
   ds_hmap_get_str_ptr (st->table, key, (void **)&existing, NULL);

   // If no existing value, our job is much simpler: set varray as the
   // new value and return success
   if (!existing) {
      if (keytype != keytype_INDEX || index != 0) {
         PARSE_ERROR (fname, lc, "%i:%zu: Expected `%s` or `%s[0]`, found `%s`\n",
               keytype, index, keycopy, keycopy, keycopy);
         goto cleanup;
      }

      if (!(ds_hmap_set_str_ptr (st->table, keycopy, varray, 0))) {
         PARSE_ERROR (fname, lc, "OOM Error creating [%s]\n", keycopy);
         goto cleanup;
      }
      varray = NULL; // Don't free this in cleanup
      error = false;
      goto cleanup;
   }

   // There is already an existing value. We must set the correct
   // indexed element within it.

   // If the index is out of bounds, error out of this function
   size_t nstrings = strarray_length (existing);
   if (index >= nstrings) {
      PARSE_ERROR (fname, lc, "Out of bounds write to `%s` at index %zu\n",
            keycopy, index);
      goto cleanup;
   }
   // If the passed in value is an array, refuse to store array within an
   // array
   size_t varray_len = strarray_length (varray);
   if (varray_len > 1) {
      PARSE_ERROR (fname, lc, "Cannot insert array `%s` into array at `%s[%zu]`\n",
            value, keycopy, index);
      goto cleanup;
   }
   // If the value is blank (user wants to only clear out existing value)
   // then truncate the value that is stored
   if (varray_len == 0) {
      existing[index][0] = 0;
      error = false;
      goto cleanup;
   }
   // Otherwise, free the existing value and replace it with the new value
   free (existing[index]);
   if (!(existing[index] = ds_str_dup (varray[0]))) {
      PARSE_ERROR (fname, lc, "OOM error copying varray[0]: `%s`\n", varray[0]);
      goto cleanup;
   }

   error = false;
cleanup:
   strarray_del (varray);
   free (keycopy);
   return !error;
}

static bool symtab_append (const char *fname, size_t lc,
                           struct symtab_t *st, const char *key, char *value)
{
   bool error = true;
   char **existing = NULL;

   enum {
      append_ERROR = 0,
      append_ELEMENT = 1,
      append_STRING = 2,
   } append_type = append_ERROR;

   size_t index = 0;
   enum keytype_t keytype = detect_keytype (key, &index);
   char *keycopy = ds_str_dup (key);
   if (!keycopy) {
      goto cleanup;
   }

   char *tmp = strchr (keycopy, '[');
   if (tmp) {
      *tmp = 0;
   }

   if (keytype != keytype_INDEX && keytype != keytype_ARRAY) {
      PARSE_ERROR (fname, lc, "%i:[%s]  Expected `%s` or `%s[]` or `%s[<number>]`, "
                   "found `%s` instead\n",
                   keytype, key,
                   keycopy, keycopy, keycopy, keycopy);
      goto cleanup;
   }

   ds_hmap_get_str_ptr (st->table, keycopy, (void **)&existing, NULL);
   size_t nexisting = ds_array_length (existing);
   if (keytype == keytype_INDEX && nexisting && index >= nexisting) {
      PARSE_ERROR (fname, lc, "Out of bounds write to `%s`\n", key);
      goto cleanup;
   }

   ds_hmap_remove_str (st->table, keycopy);

   if (keytype == keytype_ARRAY) {
      char *newval = ds_str_dup (value);
      if (!newval) {
         goto cleanup;
      }
      strarray_append (&existing, newval);
   }

   if (keytype == keytype_INDEX) {
      if (nexisting == 0 || existing == NULL) {
         if (!(existing = calloc (2, sizeof *existing))) {
            PARSE_ERROR (fname, lc, "Failed to allocate new array\n");
            goto cleanup;
         }
         if (!(existing[0] = ds_str_dup (""))) {
            PARSE_ERROR (fname, lc, "Failed to allocate empty string\n");
            goto cleanup;
         }
      }

      char *newval = ds_str_cat (existing[index], " ", value, NULL);
      if (!newval) {
         goto cleanup;
      }
      free (existing[index]);
      existing[index] = newval;
   }

   if (!(ds_hmap_set_str_ptr (st->table, keycopy, (void *)existing, 0))) {
      goto cleanup;
   }

   error = false;
cleanup:
   free (keycopy);
   return !error;
}


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
   struct symtab_t symtab;
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
   symtab_clear (&((*node)->symtab));

   free (*node);
}

static kbnode_t *node_new (const char *fname, size_t line,
                           const char *typename, kbnode_t *parent)
{
   bool error = true;
   enum node_type_t type = node_type_type (typename);
   if (type == node_type_UNKNOWN) {
      PARSE_ERROR (fname, line,
            "Attempt to create node of unknown type: '%s'\n", typename);
      return NULL;
   }

   errno = ENOMEM;

   kbnode_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      goto cleanup;
   }

   if (!symtab_init(&ret->symtab)) {
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

   symtab_dump (&node->symtab, outf);

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
      ERROR ("Failed to open [%s] for reading: %m\n", fname);
      goto cleanup;
   }

   // 1MB ought to be enough for everyone!
   static const size_t line_len = 1024 * 1024;

   if (!(line = malloc (line_len))) {
      PARSE_ERROR (fname, lc, "Failed to allocate buffer for input\n");
      errno = ENOMEM;
      goto cleanup;
   }

   while (!feof (inf) && !ferror (inf) && fgets (line, line_len, inf)) {
      lc++;
      if ((tmp = strchr (line, '\r'))) {
         PARSE_ERROR (fname, lc,
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
            PARSE_ERROR (fname, lc, "Mangled input [%s]\n", line);
            wcount++;
            goto cleanup;
         }
         *tmp = 0;

         if (!(current = node_new (fname, lc, &line[1], NULL))) {
            PARSE_ERROR (fname, lc,
                  "Node creation attempt failure near: '%s'\n", &line[1]);
            wcount++;
            goto cleanup;
         }

         if (!(ds_array_ins_tail (*dst, current))) {
            ERROR ("OOM appending new node %s to collection\n", line);
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
            PARSE_ERROR (fname, lc, "Failed to read name/value pair near '%s'\n",
                  line);
            wcount++;
            goto cleanup;
         }

         if (!(symtab_append (fname, lc, &current->symtab, name, value))) {
            PARSE_ERROR (fname, lc, "Failed to append value to '%s': \n",
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
            PARSE_ERROR (fname, lc, "Failed to read name/value pair near '%s'\n",
                  line);
            wcount++;
            goto cleanup;
         }
         if (!(symtab_set (fname, lc, &current->symtab, name, value))) {
            PARSE_ERROR (fname, lc, "Error setting value for '%s': %s\n",
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
      PARSE_ERROR (fname, lc, "Unrecognised pattern in input '%s'\n", line);
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



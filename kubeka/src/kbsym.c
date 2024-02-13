#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ds_str.h"
#include "ds_hmap.h"

#include "kbsym.h"
#include "kbutil.h"


#if 0
static bool match_any (int value, int sentinel, int match, ...)
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
#endif

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
struct kbsymtab_t {
   ds_hmap_t *table; // { char *: char ** }
};

void kbsymtab_dump (const kbsymtab_t *s, FILE *outf)
{
   if (!outf)
      outf = stdout;

   if (!s) {
      fprintf (outf, "NULL kbsymtab_t object\n");
      return;
   }

   char **keys = NULL;
   size_t nkeys = ds_hmap_keys (s->table, (void ***)&keys, NULL);
   if (nkeys == (size_t)-1) {
      KBWARN ("Failed to retrieve elements of symbol table\n");
      return;
   }

   char *tmp = NULL;

   for (size_t i=0; i<nkeys && keys && keys[i]; i++) {
      char **value = NULL;
      if (!(ds_hmap_get_str_ptr(s->table, keys[i], (void **)&value, NULL))) {
         KBWARN ("Failed to retrieve value for key [%s]\n", keys[i]);
      }
      free (tmp);
      if (!(tmp = kbutil_strarray_format (value))) {
         KBERROR ("OOM trying to format array [%s]\n", keys[i]);
         goto cleanup;
      }
      fprintf (outf, "   %s: %s\n", keys[i], tmp);
   }

cleanup:
   free (tmp);
   free (keys);
}


kbsymtab_t *kbsymtab_new (void)
{
   kbsymtab_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   if (!(ret->table = ds_hmap_new (512))) {
      free (ret);
      return NULL;
   }
   return ret;
}

void kbsymtab_del (kbsymtab_t *st)
{
   char **keys = NULL;
   size_t nkeys = 0;
   nkeys = ds_hmap_keys (st->table, (void ***)&keys, NULL);

   for (size_t i=0; i<nkeys && keys && keys[i]; i++) {
      char **values = NULL;
      if (!(ds_hmap_get_str_ptr (st->table, keys[i], (void **)&values, NULL))) {
         KBWARN ("Failed to get known good key [%s]\n", keys[i]);
      }
      kbutil_strarray_del (values);
   }
   free (keys);
   ds_hmap_del (st->table);
   free (st);
}

/*
static char **symtab_get (kbsymtab_t *st, const char *key)
{
   if (!st)
      return NULL;

   char **ret = NULL;
   if (!(ds_hmap_get_str_ptr (st->table, key, (void **)&ret, NULL))) {
      KBWARN ("Failed to find symbol [%s]\n", key);
   }
   return ret;
}
*/

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

enum keyclass_t {
   keyclass_UNKNOWN = 0,
   keyclass_RO,
   keyclass_SYSTEM,
   keyclass_USER,
};

static enum keyclass_t detect_keyclass (const char *key)
{
   if (key[0] == '_') {
      return keyclass_RO;
   }

   size_t keylen = strlen (key);
   for (size_t i=0; i<keylen; i++) {
      if (islower (key[i])) {
         return keyclass_USER;
      }
   }

   return keyclass_SYSTEM;
}


bool kbsymtab_set (const char *fname, size_t lc, bool force,
                   kbsymtab_t *st, const char *key, char *value)
{
   bool error = true;
   char **varray = NULL;
   char **existing = NULL;
   size_t index = (size_t)-1;
   char *keycopy = NULL;

   // Normalise the key (remove the `[]`)
   if (!(keycopy = ds_str_dup (key))) {
      KBPARSE_ERROR (fname, lc, "OOM Error copying key\n");
      goto cleanup;
   }
   char *tmp = strchr (keycopy, '[');
   if (tmp) {
      *tmp = 0;
   }

   if (!force && (detect_keyclass (keycopy)) == keyclass_RO) {
      KBPARSE_ERROR (fname, lc, "Cannot change read-only variable `%s`\n", keycopy);
      goto cleanup;
   }

   // Determine what type of key this is
   enum keytype_t keytype = detect_keytype (keycopy, &index);
   if (keytype == keytype_ERROR) {
      KBPARSE_ERROR (fname, lc, "Unrecognised key syntax: `%s`\n", value);
      goto cleanup;
   }

   // Split the value into an array of values
   if (!(varray = parse_value (value))) {
      KBPARSE_ERROR (fname, lc, "OOM trying to parse '%s'\n", value);
      goto cleanup;
   }

   // Get the existing values, if any
   ds_hmap_get_str_ptr (st->table, key, (void **)&existing, NULL);

   // If no existing value, our job is much simpler: set varray as the
   // new value and return success
   if (!existing) {
      if (keytype != keytype_INDEX || index != 0) {
         KBPARSE_ERROR (fname, lc, "%i:%zu: Expected `%s` or `%s[0]`, found `%s`\n",
               keytype, index, keycopy, keycopy, keycopy);
         goto cleanup;
      }

      if (!(ds_hmap_set_str_ptr (st->table, keycopy, varray, 0))) {
         KBPARSE_ERROR (fname, lc, "OOM Error creating [%s]\n", keycopy);
         goto cleanup;
      }
      varray = NULL; // Don't free this in cleanup
      error = false;
      goto cleanup;
   }

   // There is already an existing value. We must set the correct
   // indexed element within it.

   // If the index is out of bounds, error out of this function
   size_t nstrings = kbutil_strarray_length (existing);
   if (index >= nstrings) {
      KBPARSE_ERROR (fname, lc, "Out of bounds write to `%s` at index %zu\n",
            keycopy, index);
      goto cleanup;
   }
   // If the passed in value is an array, refuse to store array within an
   // array
   size_t varray_len = kbutil_strarray_length (varray);
   if (varray_len > 1) {
      KBPARSE_ERROR (fname, lc, "Cannot insert array `%s` into array at `%s[%zu]`\n",
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
      KBPARSE_ERROR (fname, lc, "OOM error copying varray[0]: `%s`\n", varray[0]);
      goto cleanup;
   }

   error = false;
cleanup:
   kbutil_strarray_del (varray);
   free (keycopy);
   return !error;
}

bool kbsymtab_append (const char *fname, size_t lc, bool force,
                      kbsymtab_t *st, const char *key, char *value)
{
   bool error = true;
   char **existing = NULL;

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

   if (!force && (detect_keyclass (keycopy)) == keyclass_RO) {
      KBPARSE_ERROR (fname, lc, "Cannot change read-only variable `%s`\n", keycopy);
      goto cleanup;
   }

   if (keytype != keytype_INDEX && keytype != keytype_ARRAY) {
      KBPARSE_ERROR (fname, lc, "%i:[%s]  Expected `%s` or `%s[]` or `%s[<number>]`, "
                   "found `%s` instead\n",
                   keytype, key,
                   keycopy, keycopy, keycopy, keycopy);
      goto cleanup;
   }

   ds_hmap_get_str_ptr (st->table, keycopy, (void **)&existing, NULL);
   size_t nexisting = kbutil_strarray_length (existing);
   if (keytype == keytype_INDEX && nexisting && index >= nexisting) {
      KBPARSE_ERROR (fname, lc, "Out of bounds write to `%s`\n", key);
      goto cleanup;
   }

   ds_hmap_remove_str (st->table, keycopy);

   if (keytype == keytype_ARRAY) {
      char *newval = ds_str_dup (value);
      if (!newval) {
         goto cleanup;
      }
      kbutil_strarray_append (&existing, newval);
   }

   if (keytype == keytype_INDEX) {
      if (nexisting == 0 || existing == NULL) {
         if (!(existing = calloc (2, sizeof *existing))) {
            KBPARSE_ERROR (fname, lc, "Failed to allocate new array\n");
            goto cleanup;
         }
         if (!(existing[0] = ds_str_dup (""))) {
            KBPARSE_ERROR (fname, lc, "Failed to allocate empty string\n");
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

bool kbsymtab_exists (kbsymtab_t *st, const char *key)
{
   if (!st || !key)
      return false;

   const char **keys = NULL;
   if (!(ds_hmap_keys (st->table, (void ***)&keys, NULL))) {
      KBERROR ("Failed to retrieve keys to search for '%s'\n", key);
      return false;
   }

   for (size_t i=0; keys && keys[i]; i++) {
      if ((strcmp (keys[i], key)) == 0) {
         free (keys);
         return true;
      }
   }
   free (keys);
   return false;
}


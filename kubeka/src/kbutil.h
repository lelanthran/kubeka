         /* ****************************************************** *
          * Copyright ©2024 Run Data Systems,  All rights reserved *
          *                                                        *
          * This content is the exclusive intellectual property of *
          * Run Data Systems, Gauteng, South Africa.               *
          *                                                        *
          * See the LICENSE file for more information.             *
          *                                                        *
          * ****************************************************** */



#ifndef H_KBUTIL
#define H_KBUTIL

#define KBWARN(...)     do {\
   fprintf (stderr, "Warning: ");\
   fprintf (stderr, __VA_ARGS__);\
   fflush (stderr);\
} while (0)

#define KBIERROR(...)     do {\
   fprintf (stderr, "[%s:%i] Error in %s(): ", __FILE__, __LINE__, __func__);\
   fprintf (stderr, __VA_ARGS__);\
   fflush (stderr);\
} while (0)

#define KBXERROR(...)     do {\
   fprintf (stderr, "Error: ");\
   fprintf (stderr, __VA_ARGS__);\
   fflush (stderr);\
} while (0)

#define KBPARSE_ERROR(fname,line, ...)     do {\
   fprintf (stderr, "Error in %s:%zu: ", fname, line);\
   fprintf (stderr, __VA_ARGS__);\
   fflush (stderr);\
} while (0)

#define KBPARSE_WARN(fname,line, ...)     do {\
   fprintf (stderr, "Warning in %s:%zu: ", fname, line);\
   fprintf (stderr, __VA_ARGS__);\
   fflush (stderr);\
} while (0)



#ifdef __cplusplus
extern "C" {
#endif

   char *kbutil_file_read (const char *fname);

   bool kbutil_test (const char *name, int expected_rc,
                     const char *ifname, const char *ofname, const char *efname,
                     int (*testfunc) (const char *input,
                                      const char *output));

   char **kbutil_strsplit (const char *src, char delim);
   void kbutil_strarray_del (char **sa);
   char *kbutil_strarray_format (const char **sa);
   size_t kbutil_strarray_length (const char **sa);
   char **kbutil_strarray_append (char ***dst, char *s);
   char **kbutil_strarray_copy (const char **src);

#ifdef __cplusplus
};
#endif


#endif



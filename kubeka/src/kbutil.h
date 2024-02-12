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
   fprintf (stderr, "[%s:%i] Program warning: ", __FILE__, __LINE__);\
   fprintf (stderr, __VA_ARGS__);\
} while (0)

#define KBERROR(...)     do {\
   fprintf (stderr, "[%s:%i] Program error: ", __FILE__, __LINE__);\
   fprintf (stderr, __VA_ARGS__);\
} while (0)

#define KBPARSE_ERROR(fname,line, ...)     do {\
   fprintf (stderr, "Error :%s+%zu: ", fname, line);\
   fprintf (stderr, __VA_ARGS__);\
} while (0)



#ifdef __cplusplus
extern "C" {
#endif

   char *kbutil_file_read (const char *fname);

   bool kbutil_test (const char *name,
                     const char *ifname, const char *ofname, const char *efname,
                     int (*testfunc) (const char *input,
                                      const char *output));

   void kbutil_strarray_del (char **sa);
   char *kbutil_strarray_format (char **sa);
   size_t kbutil_strarray_length (char **sa);
   char **kbutil_strarray_append (char ***dst, char *s);


#ifdef __cplusplus
};
#endif


#endif



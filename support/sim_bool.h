#if !defined(SIM_BOOL_H)

/* Boolean flag type: */
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || (defined(_MSC_VER) && _MSV_VER >= 1800)
   /* bool, true, false are keywords in C23. */
#  if __STDC_VERSION__ < 202311L || defined(_MSC_VER)
#    include <stdbool.h>
#  endif
#  if defined(TRUE)
#    undef TRUE
#  endif
#  if defined(FALSE)
#    undef FALSE
#  endif
#  define TRUE true
#  define FALSE false
   typedef bool            t_bool;
#else
   /* Not a standard compiler. */
#  if !defined(TRUE)
#    define TRUE            1
#    define FALSE           0
#  endif
   typedef int             t_bool;
#endif

#define SIM_BOOL_H
#endif
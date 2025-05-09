#if !defined(SIM_BOOL_H)

/* Boolean flag type: */
#if defined(__STDC_VERSION__)
#  if __STDC_VERSION__ >= 199901L
     /* bool, true, false are keywords in C23. */
#    if __STDC_VERSION__ < 202311L
#      include <stdbool.h>
#    endif
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
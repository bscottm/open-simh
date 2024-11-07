/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~
 * sim_printf_fmts.h
 *
 * Cross-platform printf() formats for simh data types. Refactored out to
 * this header so that these formats are available to more than SCP.
 *
 * Author: B. Scott Michel
 *
 * "scooter me fecit"
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~*/

#pragma once
#if !defined(SIM_PRINTF_H)

/* cross-platform printf() format specifiers:
 *
 * Note: MS apparently does recognize "ll" as "l" in its printf() routines, but "I64" is
 * preferred for 64-bit types.
 *
 * MinGW note: __MINGW64__ and __MINGW32__ are both defined by 64-bit gcc. Check
 * for __MINGW64__ before __MINGW32__.
 *
 * LL_FMT: long long format modifier, e.g. "%016" LL_FMT "x"
 * T_UINT64_FMT: t_uint64 format modifier, e.g. "%016" T_UINT64_FMT "x"
 * T_INT64_FMT: t_int64 format modifier, e.g., "%" T_INT64_FMT "d"
 * POINTER_FMT: Format modifier for pointers, e.g. "%08" POINTER_FMT "X"
 * 
 * The next two formats are C99-inspired, e.g., PRIu32 and PRIu64, which
 * SIMH could use if C99 were the baseline compiler standard.
 * 
 * SIM_PRIsize_t, SIM_PRIxsize_t, SIM_PRIXsize_t: size_t format for decimal
 *         and hexadecimal and HEXADECIMAL output, e.g., "%" SIM_PRIsize_t
 *         or "%" SIM_PRIXsize_t.
 * SIM_PRIssize_t: ssize_t format, e.g., "%" SIM_PRIssize_t (note there isn't
 *         a type format.)
*/

#if defined(HAVE_INTTYPES_H)
/* Yay! A C99+ compliant compiler!! */
#include <inttypes.h>
#endif

#if defined (_WIN32) || defined(_WIN64)

#  if defined(__MINGW64__) || defined(_WIN64) || defined(__WIN64)
#    define LL_FMT          "ll"
#    if defined(PRIu64)
#      define SIM_PRIsize_t   PRIu64
#    else
#      define SIM_PRIsize_t   "llu"
#    endif
#    if defined(PRIx64)
#      define SIM_PRIxsize_t  PRIx64
#    else
#      define SIM_PRIxsize_t  "llx"
#    endif
#    if defined(PRIX64)
#      define SIM_PRIXsize_t  PRIX64
#    else
#      define SIM_PRIXsize_t  "llX"
#    endif
#    if defined(PRId64)
#      define SIM_PRIssize_t  PRId64
#    else
#      define SIM_PRIssize_t  "lld"
#    endif
#  elif defined(__MINGW32__) || defined(_WIN32) || defined(__WIN32)
#    define LL_FMT          "ll"
#    define SIM_PRIsize_t   "zu"
#    define SIM_PRIxsize_t  "zx"
#    define SIM_PRIXsize_t  "zX"
#    define SIM_PRIssize_t  "zd"
#  else
     /* Graceful fail -- shouldn't ever default to this on a Windows platform. */
#    define LL_FMT          "ll"
#    if defined(PRIu32)
#      define SIM_PRIsize_t   PRIu32
#    else
#      define SIM_PRIsize_t   "lu"
#    endif
#    if defined(PRIx32)
#      define SIM_PRIxsize_t  PRIx32
#    else
#      define SIM_PRIxsize_t  "lx"
#    endif
#    if defined(PRIX32)
#      define SIM_PRIXsize_t  PRIX32
#    else
#      define SIM_PRIXsize_t  "lX"
#    endif
#    if defined(PRId32)
#      define SIM_PRIssize_t  PRId32
#    else
#      define SIM_PRIssize_t  "ld"
#    endif
#  endif

#  define T_UINT64_FMT   "I64"
#  define T_INT64_FMT    "I64"
#  define POINTER_FMT    "p"

#elif defined(__GNU_LIBRARY__) || defined(__GLIBC__) || defined(__GLIBC_MINOR__) || \
      defined(__APPLE__)

/* GNU libc (Linux) and macOS */
#  define LL_FMT          "ll"
#  define SIM_PRIsize_t   "zu"
#  define SIM_PRIxsize_t  "zx"
#  define SIM_PRIXsize_t  "zX"
#  define SIM_PRIssize_t  "zd"
#  define T_UINT64_FMT    "ll"
#  define T_INT64_FMT     "ll"
#  define POINTER_FMT     "p"

#elif defined(__VAX)

/* No 64 bit ints on VAX, nothing special about size_t */
#  define LL_FMT         "l"
#  define SIM_PRIsize_t  "u"
#  define SIM_PRIxsize_t "x"
#  define SIM_PRIXsize_t "X"
#  define SIM_PRIssize_t "d"
#  define T_UINT64_FMT   ""
#  define T_INT64_FMT    ""
#  define POINTER_FMT    ""

#else
/* Defaults. */
#  define LL_FMT         "ll"
#  define SIM_PRIsize_t  "u"
#  define SIM_PRIxsize_t "x"
#  define SIM_PRIXsize_t "X"
#  define SIM_PRIssize_t "d"
#  define T_UINT64_FMT   ""
#  define T_INT64_FMT    ""
#  define POINTER_FMT    ""
#endif


#if defined (USE_INT64) && defined (USE_ADDR64)
#  define T_ADDR_FMT      T_UINT64_FMT
#else
#  define T_ADDR_FMT      ""
#endif

#if defined (USE_INT64)
#  define T_VALUE_FMT     T_UINT64_FMT
#  define T_SVALUE_FMT    T_INT64_FMT
#else
#  define T_VALUE_FMT     ""
#  define T_SVALUE_FMT    ""
#endif

#define SIM_PRINTF_H
#endif

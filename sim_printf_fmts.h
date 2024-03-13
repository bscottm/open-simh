/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~
 * sim_printf_fmts.h
 *
 * Cross-platform printf() formats for simh data types. Refactored out to
 * this header so that these formats are avaiable to more than SCP.
 *
 * Author: B. Scott Michel
 *
 * "scooter me fecit"
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~*/

#pragma once
#if !defined(SIM_PRINTF_H)

/* cross-platform printf() format specifiers:
 *
 * MinGW note: __MINGW64__ and __MINGW32__ are both defined by 64-bit gcc. Check
 * for __MINGW64__ before __MINGW32__.
 *
 *
 * LL_FMT: long long format, e.g. "%016" LL_FMT
 * SIZE_T: size_t format, e.g., "%" SIZE_T_FMT
 * T_UINT64_FMT: t_uint64 format, e.g. "%016" T_UINT64_FMT
 * T_INT64_FMT: t_int64 format, e.g., "%" T_INT64_FMT
 * POINTER_FMT: Format for pointers, e.g. "%08" POINTER_FMT
*/

#if defined (_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WIN64__)

/* For the PRIxxx format specifiers... */
#include <inttypes.h>

#  if defined(_WIN64) || defined(__WIN64__)
#    define LL_FMT      PRIu64
#    define SIZE_T_FMT  PRIu64
#    define POINTER_FMT PRIuPTR
#  elif defined(_WIN32) || defined(__WIN32__)
#    define LL_FMT      PRIu32
#    define SIZE_T_FMT  PRIu32
#    define POINTER_FMT PRIuPTR
#  else
     /* Graceful fail -- shouldn't ever default to this on a Windows platform. */
#    define LL_FMT      "lld"
#    define SIZE_T_FMT  "I32u"
#    define POINTER_FMT "I32x"
#  endif

#  define T_UINT64_FMT   PRIu64
#  define T_INT64_FMT    PRIi64

#elif defined(__GNU_LIBRARY__) || defined(__GLIBC__) || defined(__GLIBC_MINOR__) || \
      defined(__APPLE__)

/* GNU libc (Linux) and macOS */
#  define LL_FMT         "lld"
#  define SIZE_T_FMT     "zu"
#  define T_UINT64_FMT   "llu"
#  define T_INT64_FMT    "lld"
#  define POINTER_FMT    "px"

#elif defined(__VAX)

/* No 64 bit ints on VAX, nothing special about size_t */
#  define LL_FMT         "ld"
#  define SIZE_T_FMT     "u"
#  define T_UINT64_FMT   "u"
#  define T_INT64_FMT    "u"
#  define POINTER_FMT    "x"

#else
/* Defaults. */
#  define LL_FMT         "lld"
#  define SIZE_T_FMT     "zu"
#  define T_UINT64_FMT   "llu"
#  define T_INT64_FMT    "lld"
#  define POINTER_FMT    "px"
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

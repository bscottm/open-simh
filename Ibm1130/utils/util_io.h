// util_io.h - definitions for simulator io routines

#include <stdio.h>
#include <string.h>

/* Constant/fixed size array size macro: */
#if defined(arraysize)
#undef arraysize
#endif

#define arraysize(arr) (sizeof(arr) / sizeof(arr[0]))

void util_io_init(void);
size_t fxread  (void *bptr, size_t size, size_t count, FILE *fptr);
size_t fxwrite (void *bptr, size_t size, size_t count, FILE *fptr);

/* Bounds-checked sprintf() */
int util_sprintf(char *buf, size_t bufsiz, const char *fmt, ...);

/* Deal with MSVCRT "secure" CRT function interoperability: */
#if !defined(_WIN32)
#define util_fopen fopen
#define util_fileno fileno
#define util_unlink unlink
#define util_filelength filelength
#define util_ctime ctime

/* sscanf_s takes an extra argument per "%s" or "%c". sscanf doesn't. */
#define scanf_str(x, size_x) x
#define util_sscanf sscanf
#else
#include <time.h>

#define util_fileno _fileno
#define util_unlink _unlink
#define util_filelength _filelength

#define scanf_str(x, size_x) x,((unsigned int) (size_x))
#define util_sscanf sscanf_s

static inline FILE *util_fopen(const char *fn, const char *mode)
{
    FILE *retval;

    if (fopen_s(&retval, fn, mode) != 0)
        retval = NULL;

    return retval;
}

static inline const char *util_ctime(const time_t *t)
{
    static char retval[32];
    return (ctime_s(retval, sizeof(retval)/sizeof(retval[0]), t) == 0 ? retval : NULL);
}
#endif

/* Bounds-checked strcpy(), strcat(), strncpy(). */
static inline char *util_strcpy(char *dest, size_t dest_size, const char *src)
{
#if !defined(_WIN32)
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
#else
    strcpy_s(dest, dest_size, src);
#endif

    return dest;
}

static inline char *util_strcat(char *dest, size_t dest_size, const char *src)
{
#if !defined(_WIN32)
    strncat(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
#else
    strcat_s(dest, dest_size, src);
#endif

    return dest;
}

static inline char *util_strncpy(char *dest, size_t dest_size, const char *src, size_t n_src)
{
#if !defined(_WIN32)
    size_t n_copy = (n_src <= dest_size) ? n_src : dest_size;
    strncpy(dest, src, n_copy);
    dest[n_copy - 1] = '\0';
#else
    strncpy_s(dest, dest_size, src, _TRUNCATE);
#endif

    return dest;
}


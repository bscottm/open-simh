/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "sim_networks/openvpn/vpndefs.h"

void
argv_init(vpn_args_t *args)
{
    args->capacity = OPENVPN_CAPACITY_INIT;
    args->argc = 0;
    args->argv = (char **) calloc(args->capacity, sizeof(char *));
    args->argv_string = NULL;
}

void
argv_free(vpn_args_t *args)
{
    args->capacity = args->argc = 0;
    free(args->argv);
    args->argv = NULL;
    free(args->argv_string);
    args->argv_string = NULL;
}

static char *
argv_separate_fmt(const char *fmt, char sep, int *n_seps)
{
    char *retval = (char *) calloc(sizeof(char), strlen(fmt) + 1);

    if (retval == NULL)
        return retval;

    int parsing = 0;
    size_t i, j;

    *n_seps = 0;
    for (i = j = 0; fmt[i] != '\0'; ++i) {
        if (fmt[i] == ' ') {
            parsing = 0;
            continue;
        }

        if (!parsing) {
            (*n_seps)++;
            if (j > 0) {
                retval[j++] = sep;
            }
        }

        retval[j++] = fmt[i];
        parsing = 1;
    }

    return retval;
}

static int
argv_extend(vpn_args_t *args, char *arg)
{
    if (args->capacity == args->argc) {
        char **tmp = (char **) realloc(args->argv, args->capacity + OPENVPN_CAPACITY_INCR);

        if (tmp == NULL)
            return 0;
        
        args->capacity += OPENVPN_CAPACITY_INCR;
        args->argv = tmp;
    }

    args->argv[args->argc++] = arg;
    return 1;
}

static int
argv_vsnprintf(vpn_args_t *args, const char *fmt, va_list fmtargs)
{
    /* OpenVPN uses GS for the string separator, so why shouldn't brilliant minds
     * think alike? */
    const char sep = 0x1D;
    /* Replace spaces with sep, coalescing adjacent spaces. */
    int n_seps;
    char *fmtsep = argv_separate_fmt(fmt, sep, &n_seps);

    if (fmtsep == NULL)
        return 0;

    /* Figure out how much space we're going to need for the formatted string. */
    va_list tmpargs;

    va_copy(tmpargs, fmtargs);
    int space_needed = vsnprintf(NULL, 0, fmtsep, tmpargs);
    va_end(tmpargs);

    if (space_needed < 0)
        goto cleanup;

    args->argv_string = (char *) malloc(space_needed + 1);

    int formatted = vsnprintf(args->argv_string, space_needed + 1, fmtsep, fmtargs);
    if (formatted < 0 || formatted >= space_needed + 1)
        goto cleanup;

    char *p = args->argv_string, *q = strchr(p, sep);

    while (q != NULL) {
        *q = '\0';
        if (!argv_extend(args, p))
            goto cleanup;
        p = q + 1;
        q = strchr(p, sep);
    }

    if (!argv_extend(args, p) || args->argc != n_seps)
        goto cleanup;

    argv_extend(args, NULL);
    return 1;

cleanup:
    free(fmtsep);
    argv_free(args);
    return 0;
}

int argv_printf(vpn_args_t *args, const char *fmt, ...)
{
    va_list fmtargs;
    int retval;

    va_start(fmtargs, fmt);
    retval = argv_vsnprintf(args, fmt, fmtargs);
    va_end(fmtargs);

    return retval;
}

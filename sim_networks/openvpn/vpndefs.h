/*
 *
 */

#if !defined(_SIM_NETWORKING_OPENVPN_VPNDEFS_H)

#include <windows.h>
#include <winioctl.h>

#include <stdint.h>
#include <stdbool.h>

#include <sys/types.h>

/* Windows driver information. This is a struct in case it ever
 * needs to be extended. */
typedef struct {
    /* Adapter driver name */
    const char *adapter_driver;
} vpn_driver_t;

/*!
 * argc/argv container, adapted from OpenVPN's argv_printf()
 */
typedef struct {
    /* Total number of elements that can be stored. */
    size_t capacity;
    /* Current number of arguments. */
    size_t argc;
    /* The arguments array. */
    char **argv;
    /* The argument string into which argv[] points. */
    char *argv_string;
} vpn_args_t;

/* Initial vpn_args_t capacity */
#define OPENVPN_CAPACITY_INIT 16
/* Incremental vpn_args_t capacity increase when needed. */
#define OPENVPN_CAPACITY_INCR 8

/* Initialize a vpn_args_t container. */
void argv_init(vpn_args_t *args);
/* Finalize a vpn_args_t container. */
void argv_free(vpn_args_t *args);
/* Argument printf(). This is inspired and adapted from OpenVPN. Format the
 * printf() string and reparse it as a series of space-separated arguments.
 */
int argv_printf(vpn_args_t *args, const char *fmt, ...);

#define _SIM_NETWORKING_OPENVPN_VPNDEFS_H
#endif
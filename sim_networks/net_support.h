/* net_support.h
 *
 * Internal header file for simulated Ethernet support. These functions
 * are only (and intended to be) used by the various "drivers" in
 * sim_networks.
 */

#if !defined(_SIM_NETSUPPORT_H)

#if defined(_WIN32) || defined(_WIN64)
#include <iphlpapi.h>
#endif

/* Visible functions across all platforms: */
int netsupport_poll_socket(SOCKET sock, int ms_timeout);

void default_reader_shutdown(eth_apidata_t *api_data);
void default_writer_shutdown(eth_apidata_t *api_data);


/* WSAPoll() is available on Windows Vista (> 0x0600) and higher, otherwise revert to
 * select(). */
#if SIM_USE_POLL && defined(WINVER) && WINVER < 0x0600
#  undef SIM_USE_SELECT
#  undef SIM_USE_POLL
#  define SIM_USE_SELECT 1
#  define SIM_USE_POLL 0
#endif

#if !defined(SIM_USE_SELECT) && !defined(SIM_USE_POLL)
#  error "sim_slirp.c: Configuration error: define SIM_USE_SELECT, SIM_USE_POLL"
#endif

#if SIM_USE_SELECT + SIM_USE_POLL > 1
#  error "sim_slirp.c: Configuration error: set one of SIM_USE_SELECT, SIM_USE_POLL to 1."
#endif

#if SIM_USE_POLL
/* Abstract the poll structure as sim_pollfd_t */
#  if !defined(_WIN32) && !defined(_WIN64)
#   include <poll.h>
    typedef struct pollfd sim_pollfd_t;
#  else
    typedef WSAPOLLFD sim_pollfd_t;

    /* poll() wrapper for Windows for uniformity: */
    static inline int poll(WSAPOLLFD *fds, size_t n_fds, int timeout)
    {
        return WSAPoll(fds, (ULONG) n_fds, timeout);
    }
#  endif
#elif SIM_USE_SELECT
#define SIM_INVALID_MAX_FD ((slirp_os_socket) -1)
#endif

#if defined(_WIN32) || defined(_WIN64)

/* C99-style print format specifiers for Windows-specific types. */
#  if defined(_UNICODE)
#    error "_UNICODE defined. SIMH uses single byte character (SBCS) sets."
#  else
#    define PRIsLPSTR       "s"
#    define PRIsLPOLESTR    "ls"
#  endif

/* Utility function definitions: */
extern const GUID GUID_DEVCLASS_NET;
extern const GUID GUID_EMPTY_GUID;

/* Registry key template for network adapter info. */
extern const char szAdapterRegKeyPathTemplate[];
#define ADAPTER_REGKEY_PATH_MAX (_countof("SYSTEM\\CurrentControlSet\\Control\\Network\\") - 1 + 38 + _countof("\\") - 1 + 38 + _countof("\\Connection"))

/* "netsh" command subpath. For the full command path, append this value
 * to windowsSystemRoot(). */
const char netshCommandPathSuffix[];

const char *windowsSystemRoot();
const IP_ADAPTER_ADDRESSES *windowsNetworkAdapterList();

/* Bulk deallocate the adapterListHeap and cleanup adapters. */
void windowsNetworkAdapterListCleanup();

/* Update the network adapter list, such as after deleting remnant IPv4
 * addresses. */
const IP_ADAPTER_ADDRESSES *windowsNetworkAdapterListUpdate();

/* Get the IP_ADAPTER_ADDRESSES element corresponding to an adapter interface index. 
 * Avoids lookup by GUID (AdapterName). */
const IP_ADAPTER_ADDRESSES *adapterByIndex(IF_INDEX index);

/* Retrieve a registry REG_SZ string or REG_MULTI_SZ (concatenated NUL-terminated strings) */
t_stat queryWindowsRegistry(_In_ HKEY hKey, _In_ LPSTR szName, _Out_ LPSTR *pszValue);
#endif

/* C99-style printf formats for sockets: */
#if defined(_WIN64)
#  if defined(PRIu64)
#    define SIM_PRIsocket PRIu64
#  else
#    define SIM_PRIsocket "llu"
#  endif
#elif defined(_WIN32)
#  if defined(PRIu32)
#    define SIM_PRIsocket PRIu32
#  else
#    define SIM_PRIsocket "u"
#  endif
#else
#  define SIM_PRIsocket "u"
#endif

#define _SIM_NETSUPPORT_H
#endif

/* sim_slirp_network.h: Header file for the SimSlirpNetwork structure.
 *
 * Previously, this structure was locally defined in sim_slirp.c, which is
 * inconvenient when referring to the structure members (scroll up to refer,
 * scroll down to code.) Broken out as its own entity.
 */

#include "sim_sock.h"
#include "sim_slirp.h"
#include "sim_atomic.h"
#include "sim_networks/net_support.h"

#if !defined(SIM_SLIRP_NETWORK_H)

#if !defined(USE_READER_THREAD)
#define pthread_mutex_init(mtx, val)
#define pthread_mutex_destroy(mtx)
#define pthread_mutex_lock(mtx)
#define pthread_mutex_unlock(mtx)
#define pthread_mutex_t int
#endif

/* sim_slirp debugging: */
enum {
    DBG_POLL         = 0,
    DBG_SOCKET       = 1
};
struct sim_slirp {
    SlirpConfig  slirp_config;
    SlirpCb      slirp_callbacks;
    Slirp       *slirp_cxn;

    char *args;

#if defined(USE_READER_THREAD)
    /* Access lock to libslirp. libslirp is not threaded or protected. */
    pthread_mutex_t libslirp_lock;

    /* Condvar, mutex when there are no sockets to poll or select. */
    pthread_cond_t  no_sockets_cv;
    pthread_mutex_t no_sockets_lock;
#endif

    sim_atomic_value_t n_sockets;

    /* DNS search domains (argument copy) */
    char *dns_search;
    char **dns_search_domains;
    /* Boot file and TFTP path prefix (argument copy) */
    char *the_bootfile;
    char *the_tftp_path;
    /* UDP and TCP ports that SIMH proxies to the Slirp network */
    struct redir_tcp_udp *rtcp;
    /* SIMH's underlying Ethernet device state */
    ETH_DEV *eth_dev;

    /* I/O event tracking/handling (used to be the GPollFD array): */
#if SIM_USE_SELECT
    /* select() needs a lookup table to map SOCKETs to an integer index. */
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    slirp_os_socket max_fd;

    /* Lookup table: */
    slirp_os_socket *lut;
    size_t lut_alloc;
#elif SIM_USE_POLL
    /* Next descriptor to use */
    size_t fd_idx;
    /* Total allocated descriptors */
    size_t n_fds;
    /* Poll file descriptor array */
    sim_pollfd_t *fds;
#endif

    /* SIMH debug info: */
    DEVICE *dptr;
    uint32 dbit;
};

/* Simulator -> host network redirection state. */
struct redir_tcp_udp {
    int is_udp;
    /* SIMH host port, e.g., 2223.  */
    int simh_host_port;
    /* The simulator's IP address, e.g., 10.0.2.4 or 10.0.2.15 */
    struct in_addr sim_local_inaddr;
    /* The simulator's port, e.g., 23 */
    int sim_local_port;
    struct redir_tcp_udp *next;
};

/* File descriptor array initial allocation, incremental (linear) allocation. */
#define FDS_ALLOC_INIT 32
#define FDS_ALLOC_INCR 32

/* slirp_poll.c externs: */
int sim_slirp_send (SimSlirpNetwork *slirp, const char *msg, size_t len, int flags);
void sim_slirp_shutdown(SimSlirpNetwork *slirp);
int sim_slirp_send (SimSlirpNetwork *slirp, const char *msg, size_t len, int flags);
int sim_slirp_select (SimSlirpNetwork *slirp, int ms_timeout);

void register_poll_socket(slirp_os_socket fd, void *opaque);
void unregister_poll_socket(slirp_os_socket fd, void *opaque);

void *simh_timer_new_opaque(SlirpTimerId id, void *cb_opaque, void *opaque);
void simh_timer_free(void *the_timer, void *opaque);
void simh_timer_mod(void *timer, int64_t expire_time, void *opaque);
int64_t sim_clock_get_ns(void *opaque);

#define SIM_SLIRP_NETWORK_H
#endif


#include "sim_defs.h"
#include "sim_sock.h"
#include "sim_ether.h"
#include "sim_networks/sim_networks.h"
#include "sim_networks/net_support.h"

int netsupport_poll_socket(SOCKET sock, int ms_timeout)
{
    int retval = 0;

#  if SIM_USE_SELECT
    fd_set setl;
    struct timeval timeout;
#    if defined(_WIN32) || defined(_WIN64)
    /* select() on Windows ignores the n_fd parameter, so feed it a dummy
     * value. Avoids compiler warnings re: truncated types on Win64. */
    const int n_fds = 0xcafef00d;
#    else
    const int n_fds = sock + 1;
#    endif

    FD_ZERO(&setl);
    FD_SET(sock, &setl);
    timeout.tv_sec = 0;
    timeout.tv_usec = ms_timeout * 1000;

    retval = select(n_fds, &setl, NULL, NULL, &timeout);
#  elif SIM_USE_POLL
    sim_pollfd_t poll_fd = {
        .fd = sock,
        .events = POLLIN,
        .revents = 0
    };

#    if !defined(_WIN32) && !defined(_WIN64)
    /* WSAPoll will return EINVAL if these are set. */
    poll_fd.events |= POLLERR | POLLHUP;;
#    endif

    retval = poll(&poll_fd, 1, ms_timeout);
#  else
#    error "sim_ether.c/poll_socket: Configuration error: define SIM_USE_SELECT, SIM_USE_POLL"
#  endif

    return retval;
}

/*============================================================================*/
/* Default shutdown functions                                                 */
/*============================================================================*/

void default_reader_shutdown(eth_apidata_t *api_data)
{
    SIM_UNUSED_ARG(api_data);
}

void default_writer_shutdown(eth_apidata_t *api_data)
{
    SIM_UNUSED_ARG(api_data);
}

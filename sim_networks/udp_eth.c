#include "sim_defs.h"
#include "sim_sock.h"
#include "sim_ether.h"
#include "sim_networks/sim_networks.h"
#include "sim_networks/net_support.h"

static int udp_reader(ETH_DEV *eth_dev, int ms_timeout);
static int udp_writer(ETH_DEV *eth_dev, ETH_PACK *packet);

const eth_apifuncs_t udp_api_funcs = {
    .reader = udp_reader,
    .writer = udp_writer,
    .reader_shutdown = default_reader_shutdown,
    .writer_shutdown = default_writer_shutdown
};

int udp_reader(ETH_DEV *eth_dev, int ms_timeout)
{
    int retval = 1;

#if defined(USE_READER_THREAD)
    retval = netsupport_poll_socket(eth_dev->api_data.udp_sock, ms_timeout);
#endif

    if (retval > 0) {
        int len;
        u_char buf[ETH_MAX_JUMBO_FRAME];

        len = (int) sim_read_sock (eth_dev->api_data.udp_sock, (char *) buf, (int32) sizeof(buf));
        if (len > 0) {
            sim_eth_callback(eth_dev, len, len, buf);
        }

        /* retval evaluates to -1 (len < 0), 1 (len > 0) or 0 (len == 0) */
        retval = (len < 0) * -1 + (len > 0) * 1;
    }

    return retval;
}

int udp_writer(ETH_DEV *eth_dev, ETH_PACK *packet)
{
  return (((int) packet->len == sim_write_sock(eth_dev->api_data.udp_sock, (char *) packet->msg, (int) packet->len)) ? 0 : -1);
}

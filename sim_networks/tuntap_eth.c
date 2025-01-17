
#include "sim_ether.h"
#include "sim_networks/net_support.h"
#include "sim_networks/sim_networks.h"

/*=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~*/
/* *nix TUNTAP-based simulated Ethernet implementation:                       */
/*=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~*/

#if defined(HAVE_TAP_NETWORK)
static int tuntap_reader(ETH_DEV* eth_dev, int ms_timeout);
static int tuntap_writer(ETH_DEV* eth_dev, ETH_PACK* packet);

/* TUN/TAP API functions*/
const eth_apifuncs_t tuntap_api_funcs = {
    .reader = tuntap_reader,
    .writer = tuntap_writer,
    .reader_shutdown = default_reader_shutdown,
    .writer_shutdown = default_writer_shutdown,
};

t_stat tuntap_open(const char* devname, ETH_DEV* dptr, char errbuf[PCAP_ERRBUF_SIZE], char* bpf_filter, void* opaque,
                   DEVICE* parent_dev, uint32 dbit)
{
    int tun = -1; /* TUN/TAP Socket */
    int on = 1;

#if (defined(__linux) || defined(__linux__)) && defined(HAVE_TAP_NETWORK)
    if ((tun = open("/dev/net/tun", O_RDWR)) >= 0) {
        struct ifreq ifr; /* Interface Requests */

        memset(&ifr, 0, sizeof(ifr));
        /* Set up interface flags */
        strlcpy(ifr.ifr_name, devname, sizeof(ifr.ifr_name));
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

        /* Send interface requests to TUN/TAP driver. */
        if (ioctl(tun, TUNSETIFF, &ifr) >= 0) {
            if (ioctl(tun, FIONBIO, &on)) {
                strlcpy(errbuf, strerror(errno), PCAP_ERRBUF_SIZE);
                close(tun);
                tun = -1;
            } else {
                dptr->api_data.tap_sock = tun;
                strcpy(savname, ifr.ifr_name);
            }
        } else
            strlcpy(errbuf, strerror(errno), PCAP_ERRBUF_SIZE);
    } else
        strlcpy(errbuf, strerror(errno), PCAP_ERRBUF_SIZE);
    if ((tun >= 0) && (errbuf[0] != 0)) {
        close(tun);
        tun = -1;
    }
#elif defined(HAVE_BSDTUNTAP) && defined(HAVE_TAP_NETWORK)
    char dev_name[64] = "";

    snprintf(dev_name, sizeof(dev_name) - 1, "/dev/%s", devname);
    dev_name[sizeof(dev_name) - 1] = '\0';

    if ((tun = open(dev_name, O_RDWR)) >= 0) {
        if (ioctl(tun, FIONBIO, &on)) {
            strlcpy(errbuf, strerror(errno), PCAP_ERRBUF_SIZE);
            close(tun);
            tun = -1;
        } else {
            dptr->api_data.tap_sock = tun;
            memmove(savname, devname, strlen(devname) + 1);
        }
#if defined(__APPLE__)
        if (tun >= 0) { /* Good so far? */
            struct ifreq ifr;
            int s;

            /* Now make sure the interface is up */
            memset(&ifr, 0, sizeof(ifr));
            ifr.ifr_addr.sa_family = AF_INET;
            strlcpy(ifr.ifr_name, savname, sizeof(ifr.ifr_name));
            if ((s = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
                if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) >= 0) {
                    ifr.ifr_flags |= IFF_UP;
                    if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr)) {
                        strlcpy(errbuf, strerror(errno), PCAP_ERRBUF_SIZE);
                        close(tun);
                        tun = -1;
                    }
                }
                close(s);
            }
        }
#endif
    } else
        strlcpy(errbuf, strerror(errno), PCAP_ERRBUF_SIZE);

    if ((tun >= 0) && (errbuf[0] != 0)) {
        close(tun);
        tun = -1;
    }
#else
    strlcpy(errbuf, "Operating system does not support tap: devices", PCAP_ERRBUF_SIZE);
#endif

    if (0 == errbuf[0]) {
        dptr->eth_api = ETH_API_TAP;
        dptr->api_funcs = tuntap_api_funcs;
        return SCPE_OK;
    }

    return SCPE_OPENERR;
}

int tuntap_reader(ETH_DEV* eth_dev, int ms_timeout)
{
#if defined(USE_READER_THREAD)
    int retval = netsupport_poll_socket(eth_dev->api_data.tap_sock, ms_timeout);
#else
    /* Non-blocking/non-AIO needs a value to get past the conditional. */
    int retval = 1;
#endif

    if (retval > 0) {
        int len;
        u_char buf[ETH_MAX_JUMBO_FRAME];

        len = read(eth_dev->api_data.tap_sock, buf, sizeof(buf));
        if (len > 0) {
            sim_eth_callback(eth_dev, len, len, buf);
        }

        /* retval evaluates to -1 (len < 0), 1 (len > 0) or 0 (len == 0) */
        retval = (len < 0) * -1 + (len > 0) * 1;
    }

    return retval;
}

int tuntap_writer(ETH_DEV* eth_dev, ETH_PACK* packet)
{
    return (((int)packet->len == write(eth_dev->api_data.tap_sock, (void*)packet->msg, packet->len)) ? 0 : -1);
}
#endif /* HAVE_TAP_NETWORK */

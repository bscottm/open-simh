/* sim_networks.h
 *
 * Principal header for simulated Ethernet support "drivers", e.g., PCAP,
 * libslirp, VDE, OpenVPN and UDP.
 */

#if !defined(_SIM_NETWORKING_H)

#if defined(HAVE_PCAP_NETWORK)
t_stat sim_pcap_open(eth_apidata_t *pcap_api, const char *dev_name, int bufsz, char *errbuf);
void sim_pcap_close(eth_apidata_t *pcap_api);

/* Non-AIO code directly uses the npcap/libpcap packet reciever callback. */
// void pcap_callback(u_char *eth_opaque, const struct pcap_pkthdr *header, const u_char *data);

extern const eth_apifuncs_t pcap_api_funcs;
#endif

#if defined(HAVE_SLIRP_NETWORK)
SimSlirpNetwork *sim_slirp_open (const char *args, ETH_DEV *eth_dev, DEVICE *dptr, uint32 dbit, char *errbuf, size_t errbuf_size);
void sim_slirp_close (SimSlirpNetwork *slirp);

extern const DEBTAB slirp_dbgtable[];
extern const eth_apifuncs_t slirp_api_funcs;
#endif

#if defined(HAVE_TAP_NETWORK)
extern const eth_apifuncs_t tuntap_api_funcs;
#endif

#if defined(WITH_OPENVPN_TAPTUN)

/* "show eth" support function that retrieves the OpenVPN tap-windows devices. */
t_stat openvpn_tap_devices(ETH_LIST *ethDevices, const int maxList);
/* Parse command line parameters and open the OpenVPN tap device */
t_stat openvpn_open(const char *cmdstr, ETH_DEV *dev, char errbuf[PCAP_ERRBUF_SIZE], char *bpf_filter,
                    void *opaque, DEVICE *parent_dev, uint32 dbit);

extern const eth_apifuncs_t openvpn_api_funcs;
#endif

/* VDE-based Ethernet */
#if defined(HAVE_VDE_NETWORK)
/* Only need to expose the API functions.*/
extern const eth_apifuncs_t vde_api_funcs;
#endif

/* UDP-based Ethernet is always built in: */
extern const eth_apifuncs_t udp_api_funcs;

#if defined(_WIN32) || defined(_WIN64)
/* Utility function used by eth_host_pcap_devices() to get the Windows interface
 * description. */
t_stat windows_eth_dev_description(const char *dev_guid, char **description);

/* Utility function that looks up the adapter's brace-enclosed GUID and returns
 * the associated Ethernet MAC address. */
int pcap_mac_if_win32(const char *AdapterName, unsigned char MACAddress[6]);
#endif

#define _SIM_NETWORKING_H
#endif

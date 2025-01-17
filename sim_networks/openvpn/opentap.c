/*=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~
 * OpenVPN-based TAP simulated Ethernet support
 *
 * Definitions for using the OpenVPN TAP device for simulated Ethernet support.
 *
 * This code is adapted from the OpenVPN "openvpn" utility's code, GitHub
 * repository: https://github.com/OpenVPN/openvpn
 *
 * Note that while OpenVPN's code is licensed as GPLv2, which requires the
 * source code be published or made available to the end user, SIMH is licensed
 * with MIT, which is compatible with GPLv2 for the intent and purpose of
 * source code availability compliance.
 *
 * (c) 2025 B. Scott Michel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of The Authors shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from the Authors.
 *=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~*/

#if defined(WITH_OPENVPN_TAPTUN)

#define WINDOWS_LEAN_AND_MEAN 1
#include <ws2tcpip.h>
#include <windows.h>
#include <cfgmgr32.h>
#include <tap-windows.h>

#include "sim_defs.h"
#include "sim_ether.h"
#include "sim_fio.h"
#include "sim_networks/net_support.h"
#include "sim_networks/openvpn/vpndefs.h"
#include "sim_networks/sim_networks.h"
#include "sim_sock.h"

#include "opentap.h"

/* Forward declarations: */
static HANDLE open_from_guid(const GUID* adapterGUID, char* errbuf, size_t errbuf_size);
static int initialize_overlapped_io(tap_state_t* tstate);
static int set_forwarding(const ETH_LIST* const dev);
static int set_enabled(HANDLE tapdev);

/* Parse the SIMH command line string and open the TAP device.
 *
 * Returns:
 * SCPE_OK: Success.
 * SCPE_OPENERR: Unsuccessful.
 */
t_stat openvpn_open(const char* cmdstr, ETH_DEV* dptr, char errbuf[PCAP_ERRBUF_SIZE], char* bpf_filter, void* opaque,
                    DEVICE* parent_dev, uint32 dbit)
{
    const char *devstr = cmdstr + 4, *tuntap_devname;
    int ndevs;
    size_t i;
    ETH_LIST dev_list[ETH_MAX_DEVICE];

    ndevs = eth_devices(ETH_MAX_DEVICE, dev_list, FALSE);

    while (isspace(*devstr))
        ++devstr;

    /* Get the TAP device's GUID */
    if (*devstr == '"') {
        tuntap_devname = ++devstr;
        while (*devstr && *devstr != '"')
            ++devstr;
        if (!*devstr || *devstr != '"') {
            strlcpy(errbuf, "Unterminated tap device name string.\n", PCAP_ERRBUF_SIZE);
            return SCPE_OPENERR;
        }
    } else {
        tuntap_devname = devstr;
        while (*devstr && !isspace(*devstr))
            ++devstr;
    }

    /* Look in the metadata for the TAP device. */
    HANDLE tapdev = INVALID_HANDLE_VALUE;
    for (i = 0; i < ndevs; ++i) {
        size_t name_len = devstr - tuntap_devname;

        if (dev_list[i].is_openvpn && memcmp(&dev_list[i].adapter_guid, &GUID_EMPTY_GUID, sizeof(GUID))) {
            if (name_len == 0) {
                /* Empty name -- try ever available OpenVPN adapter. */
                tapdev = open_from_guid(&dev_list[i].adapter_guid, errbuf, PCAP_ERRBUF_SIZE);
                if (tapdev != INVALID_HANDLE_VALUE)
                    break;
            } else if (strlen(dev_list[i].name) == name_len &&
                       !memcmp(dev_list[i].name, tuntap_devname, devstr - tuntap_devname)) {
                tapdev = open_from_guid(&dev_list[i].adapter_guid, errbuf, PCAP_ERRBUF_SIZE);
                break;
            }
        }
    }

    if (i >= ndevs) {
        if (devstr - tuntap_devname > 0)
            strlcpy(errbuf, "No such OpenVPN TAP device.\n", PCAP_ERRBUF_SIZE);
        else
            strlcpy(errbuf, "No available OpenVPN devices.\n", PCAP_ERRBUF_SIZE);

        return SCPE_OPENERR;
    }

    if (tapdev != INVALID_HANDLE_VALUE) {
        tap_state_t* tstate = &dptr->api_data.openvpn.tap_state;
        ULONG tap_version[3];
        /* AA:BB:CC:DD:EE:FF\0 */
        char macdaddy[_countof(tstate->adapter_mac) * 2 + 5 + 1];

        tstate->tap_dev = tapdev;

        if (FAILED(DeviceIoControl(tapdev, TAP_WIN_IOCTL_GET_VERSION, NULL, 0, tap_version, sizeof(tap_version), NULL, NULL)))
            goto open_error;

        /* Grab the adapter's MAC address. We'll use this later when sending and receiving packets. */
        if (FAILED(DeviceIoControl(tapdev, TAP_WIN_IOCTL_GET_MAC, NULL, 0, &tstate->adapter_mac, sizeof(&tstate->adapter_mac),
                                   NULL, NULL)))
            goto open_error;

        eth_mac_fmt(tstate->adapter_mac, macdaddy);
        sim_messagef(SCPE_OK, "TAP-Windows driver version %lu.%lu.%lu @ MAC address %s\n", tap_version[0], tap_version[1],
                     tap_version[2], macdaddy);

        set_forwarding(dev_list + i);
        initialize_overlapped_io(tstate);
        set_enabled(tapdev);

        /* Ready to go... */
        dptr->eth_api = ETH_API_TAP;
        dptr->api_funcs = openvpn_api_funcs;
    } else {
        strlcpy(errbuf, "Unable to open OpenVPN TAP device.\n", PCAP_ERRBUF_SIZE);
        return SCPE_OPENERR;
    }

    return SCPE_OK;

open_error:
    DWORD dwErrCode = GetLastError();

    CloseHandle(tapdev);
    strlcpy(errbuf, sim_get_os_error_text(dwErrCode), PCAP_ERRBUF_SIZE);
    return sim_messagef(SCPE_OPENERR, "Error %08x: %s\n", dwErrCode, errbuf);
}

/* Open the OpenVPN TAP device: */
HANDLE
open_from_guid(const GUID* adapterGUID, char* errbuf, size_t errbuf_size)
{
    OLECHAR szTapGUID[40];
    char tuntap_path[256];

    StringFromGUID2(adapterGUID, szTapGUID, _countof(szTapGUID));
    sprintf(tuntap_path, "%s%ls%s", USERMODEDEVICEDIR, szTapGUID, TAP_WIN_SUFFIX);

    HANDLE tapdev = CreateFile(tuntap_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                               FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);

    if (tapdev == INVALID_HANDLE_VALUE) {
        errbuf[errbuf_size - 1] = '\0';
        snprintf(errbuf, errbuf_size, "Unable to open OpenVPN TAP device %" PRIsLPOLESTR, szTapGUID);
        return tapdev;
    }

    return tapdev;
}

/* */
int clear_ipv4_addresses(const ETH_LIST* const dev) { return 0; }

/* */
int clear_ipv6_addresses(const ETH_LIST* const dev) { return 0; }

/* */
int set_forwarding(const ETH_LIST* const dev)
{
    vpn_args_t netsh_args;

    argv_init(&netsh_args);
    if (argv_printf(&netsh_args, "%s%s interface set interface %d forward", windowsSystemRoot(), netshCommandPathSuffix,
                    dev->adapter_idx)) {
        /* Do something. */
    }
    argv_free(&netsh_args);
    return 0;
}

/*!
 *
 */
int initialize_overlapped_io(tap_state_t* tstate)
{
    /* Windows event objects to signal when I/O has completed. */
    ZeroMemory(&tstate->send_overlapped, sizeof(tstate->send_overlapped));
    tstate->send_overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    ZeroMemory(&tstate->recv_overlapped, sizeof(tstate->recv_overlapped));
    tstate->recv_overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    return SCPE_OK;
}

/**
 * Set the TAP interface to its enabled (up) state.
 *
 * @param tapdev        Handle to the TAP interface.
 * @return
 */
int set_enabled(HANDLE tapdev)
{
    ULONG status = TRUE;
    DWORD len;

    if (FAILED(DeviceIoControl(tapdev, TAP_WIN_IOCTL_SET_MEDIA_STATUS, &status, sizeof(status), &status, sizeof(status), &len,
                               NULL))) {
        sim_printf("%s: DeviceIOControl TAP_WIN_IOCTL_SET_MEDIA_STATUS to TRUE failed.\n", __FUNCTION__);
        return FALSE;
    }

    return TRUE;
}

#endif

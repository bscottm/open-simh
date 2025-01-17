/*
 *
 */

#include "sim_defs.h"
#include "sim_sock.h"
#include "sim_ether.h"
#include "sim_networks/sim_networks.h"
#include "sim_networks/net_support.h"

#if defined(WITH_OPENVPN_TAPTUN)

static int openvpn_reader(ETH_DEV *eth_dev, int ms_timeout);
static int openvpn_writer(ETH_DEV *eth_dev, ETH_PACK *packet);

/* TUN/TAP API functions*/
const eth_apifuncs_t openvpn_api_funcs = {
    .dev_open = openvpn_open,
    .reader = openvpn_reader,
    .writer = openvpn_writer,
    .reader_shutdown = default_reader_shutdown,
    .writer_shutdown = default_writer_shutdown,
};

int openvpn_reader(ETH_DEV *eth_dev, int ms_timeout)
{
    tap_state_t *tap_state = &eth_dev->api_data.openvpn.tap_state;
    DWORD numXferred;

    /* Still waiting for a packet? */
    if (tap_state->recv_overlapped.Internal == STATUS_PENDING) {
        /* Overlapped I/O version of poll()/select() */
        if (GetOverlappedResultEx(tap_state->tap_dev, &tap_state->recv_overlapped, &numXferred, ms_timeout, FALSE) == 0) {
            switch (GetLastError()) {
            case WAIT_TIMEOUT:
                return 0;
            default:
                sim_messagef(SCPE_IOERR, "%s: GetOverlappedResultEx returned: %s\n",
                             __FUNCTION__, sim_get_os_error_text(GetLastError()));
                return -1;
            }
        }
    } else if (tap_state->recv_overlapped.Internal > 0) {
        /* Hmmm... encountered an error. */
        sim_messagef(SCPE_IOERR, "%s: Overlapped I/O error: %s\n",
                     __FUNCTION__, sim_get_os_error_text((int) tap_state->recv_overlapped.Internal));
        return -1;
    } else if (tap_state->recv_overlapped.Internal == 0 && tap_state->recv_overlapped.InternalHigh > 0) {
        /* Hey. We read something! Convert the MAC address from the TAP adapter's back to the
         * MAC expected by the simulator. */
        if (!memcmp(tap_state->recv_buffer, tap_state->adapter_mac, sizeof(ETH_MAC))) {
            memcpy(tap_state->recv_buffer, eth_dev->physical_addr, sizeof(ETH_MAC));
        }
        if (!memcmp(tap_state->recv_buffer + 6, tap_state->adapter_mac, sizeof(ETH_MAC))) {
            memcpy(tap_state->recv_buffer + 6, eth_dev->physical_addr, sizeof(ETH_MAC));
        }

        sim_eth_callback(eth_dev, tap_state->recv_overlapped.InternalHigh, tap_state->recv_overlapped.InternalHigh,
                         tap_state->recv_buffer);
        return 1;
    }

    /* Queue up the next read. */
    BOOL status;
    do {
        DWORD numRead;

        tap_state->recv_overlapped.Internal = 0;
        tap_state->recv_overlapped.InternalHigh = 0;
        ResetEvent(tap_state->recv_overlapped.hEvent);

        status = ReadFile(tap_state->tap_dev, tap_state->recv_buffer, _countof(tap_state->recv_buffer), &numRead,
                          &tap_state->recv_overlapped);
        if (status) {
            /* Got something to read -- process it. */
            sim_eth_callback(eth_dev, numRead, numRead, tap_state->recv_buffer);
        } else {
            /* FALSE path -- we're waiting for an asynchronous read. */
            DWORD lastError = GetLastError();
            switch (lastError) {
            case ERROR_IO_PENDING:
                /* Return so that we can come back and wait. */
                return 0;
            default:
                sim_messagef(SCPE_IOERR, "%s: ReadFile returned: %s\n",
                             __FUNCTION__, sim_get_os_error_text(lastError));
                return -1;
            }
        }
    } while (status == TRUE);

    /* Probably never reached. */
    return 0;
}

int openvpn_writer(ETH_DEV *eth_dev, ETH_PACK *packet)
{
    tap_state_t *tap_state = &eth_dev->api_data.openvpn.tap_state;
    BOOL status;
    DWORD numXferred;

    /* Have an outstanding write from a previous call? */
    if (tap_state->send_overlapped.Internal == STATUS_PENDING) {
        if (GetOverlappedResult(tap_state->tap_dev, &tap_state->send_overlapped, &numXferred, TRUE) == 0) {
            /* Failed for some reason? */
            sim_messagef(SCPE_IOERR, "%s: GetOverlappedResult returned: %s\n",
                         __FUNCTION__, sim_get_os_error_text(GetLastError()));
            return -1;
        }
    } else if (tap_state->send_overlapped.Internal != 0) {
        /* Last outstanding write ended up with an error. */
        sim_messagef(SCPE_IOERR, "%s: Overlapped I/O error: %s\n",
                     __FUNCTION__, sim_get_os_error_text(GetLastError()));
        return -1;
    }

    /* Send the packet out with the TAP adapter's MAC addresses: */
    if (!memcmp(packet->msg, eth_dev->physical_addr, sizeof(ETH_MAC))) {
        memcpy(packet->msg, tap_state->adapter_mac, sizeof(ETH_MAC));
    }
    if (!memcmp(packet->msg + 6, eth_dev->physical_addr, sizeof(ETH_MAC))) {
        memcpy(packet->msg + 6, tap_state->adapter_mac, sizeof(ETH_MAC));
    }

    /* Reset the overlapped I/O event even though we ignore its status. */
    ResetEvent(tap_state->send_overlapped.hEvent);
    /* Pull the trigger... */
    status = WriteFile(tap_state->tap_dev, packet->msg, packet->len, NULL, &tap_state->send_overlapped);
    if (status == 0) {
        if (GetLastError() != ERROR_IO_PENDING) {
            sim_messagef(SCPE_IOERR, "%s: WriteFile returned: %s\n", __FUNCTION__, sim_get_os_error_text(GetLastError()));
            return -1;
        }
    }
    
    return 0;
}
#endif /* WITH_OPENVPN_TAPTUN */
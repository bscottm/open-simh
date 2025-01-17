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

#if !defined(_SIM_OPENVPN_H)

#define OPENVPN_MAX_READ 65536

/*!
 * OpenVPN TAP device state.
 */
typedef struct tap_state_s {
    /*! Handle to the OpenVPN TAP device. */
    HANDLE tap_dev;
    /*! Windows' adapter index (IP help API [iphlpapi] and 'netsh' command) */
    DWORD adapter_index;
    /*! Adapter's MAC address (vice the simulated Ethernet adapter's MAC) */
    unsigned char adapter_mac[6];

    /*! Overlapped I/O structure for the send side. */
    OVERLAPPED send_overlapped;

    /*! Overlapped I/O structure for the receive side. */
    OVERLAPPED recv_overlapped;
    /*! Receiver buffer. */
    unsigned char recv_buffer[OPENVPN_MAX_READ];
} tap_state_t;

#define _SIM_OPENVPN_H
#endif
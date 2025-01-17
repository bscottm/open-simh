#if defined(_WIN32) || defined(_WIN64)

#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include "sim_defs.h"
#include "scp.h"
#include "sim_sock.h"
#include "sim_ether.h"
#include "sim_networks/sim_networks.h"
#include "sim_networks/net_support.h"

/* Forward declarations of Windows items that are used by OpenVPN TAPTUN and
 * the "show eth" command. */

/* MS-defined GUID for the network device class. */
const GUID GUID_DEVCLASS_NET = { 0x4d36e972L, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
const GUID GUID_EMPTY_GUID   = { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

/* String-ified GUID_DEVCLASS_NET. (TODO: CoTaskMemFree() at exit.)*/
static LPOLESTR szDevClassNetId = NULL;

/* Registry key template for network adapter info. */
const char szAdapterRegKeyPathTemplate[] = "SYSTEM\\CurrentControlSet\\Control\\Network\\%" PRIsLPOLESTR "\\%" PRIsLPOLESTR "\\Connection";

/* Windows' SystemRoot */
static char *env_windowsSystemRoot = NULL;

/* The netsh command underneath SystemRoot. */
const char netshCommandPathSuffix[] = "\\System32\\netsh.exe";


/*!
 * Windows system root path (the \$SYSTEMROOT environment variable.) If not set
 * in the environment (unlikely, but possible), set to "C:\\Windows" as the
 * default.
 * 
 * \return The value of \$SYSTEMROOT or C:\\Windows if not present in the
 * environment.
 */
const char *windowsSystemRoot()
{
    if (env_windowsSystemRoot != NULL)
        return env_windowsSystemRoot;

    DWORD envValueSize = 0, retval;

    do {
        retval = GetEnvironmentVariable("SystemRoot", env_windowsSystemRoot, envValueSize);
        if (retval == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            env_windowsSystemRoot = strdup("C:\\Windows");
            ASSURE(env_windowsSystemRoot != NULL);
            envValueSize = retval;
        } else if (retval > envValueSize) {
            envValueSize = retval;
            env_windowsSystemRoot = malloc(envValueSize);
        }
    } while (envValueSize <= retval);

    return windowsSystemRoot();
}

/*!
 * Separate allocation heap for the adapter list.
 *
 * \todo Destroy heap at exit.
 */
static HANDLE adapterListHeap = NULL;

/*!
 * Window's network adapter information linked list. Allocated from the
 * adapterListHeap.
 */
static IP_ADAPTER_ADDRESSES *adapters = NULL;

/*!
 * Get Window's network interface info list. This list is dynamically allocated,
 * needs to be free()-ed by the caller.
 * 
 * \return An IP_ADAPTER_ADRESSES linked list of adapter metadata info.
 */
const IP_ADAPTER_ADDRESSES *
windowsNetworkAdapterList()
{
    if (adapters != NULL)
        return adapters;

    DWORD dwResult;    
    ULONG nAdapters = 0;

    dwResult = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, NULL, adapters, &nAdapters);
    if (dwResult != ERROR_BUFFER_OVERFLOW) {
        sim_messagef(SCPE_IOERR, "%s: GetAdaptersAddresses failed (status=%u) : %s",
                     __FUNCTION__, (unsigned int) dwResult, sim_get_os_error_text(dwResult));
        return NULL;
    }

    adapterListHeap = HeapCreate(0, nAdapters, nAdapters);
    if (adapterListHeap == NULL) {
        sim_messagef(SCPE_MEM, "%s: HeapCreate failed : %s", __FUNCTION__, sim_get_os_error_text(GetLastError()));
        return NULL;
    }

    adapters = (IP_ADAPTER_ADDRESSES *) HeapAlloc(adapterListHeap, 0, nAdapters);
    if (adapters == NULL) {
        sim_messagef(SCPE_MEM, "%s: HeapAlloc failed : %s", __FUNCTION__, sim_get_os_error_text(GetLastError()));
        return NULL;
    }

    dwResult = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, NULL, adapters, &nAdapters);
    if (dwResult == NO_ERROR)
        return adapters;

    sim_messagef(SCPE_IOERR, "%s: GetAdapterAddresses (2) failed (status=%u) : %s",
                 __FUNCTION__, (unsigned int) dwResult, sim_get_os_error_text(dwResult));
    return NULL;
}

void 
windowsNetworkAdapterListCleanup()
{
    HeapDestroy(adapterListHeap);
    adapterListHeap = NULL;
    adapters = NULL;
}

const IP_ADAPTER_ADDRESSES *
windowsNetworkAdapterListUpdate()
{
    windowsNetworkAdapterListCleanup();
    return windowsNetworkAdapterList();

}

const IP_ADAPTER_ADDRESSES *
adapterByIndex(IF_INDEX index)
{
    const IP_ADAPTER_ADDRESSES *adapter;

    for (adapter = windowsNetworkAdapterList(); adapter != NULL; adapter = adapter->Next) {
        if (adapter->IfIndex == index)
            return adapter;
    }

    return NULL;
}


/*!
 * Query a name's value within a Windows Registry key.
 *
 * \param hKey      The opened registry key, e.g., HKLM\Microsoft\yada\yada 
 * \param szName    The name to query within the registry key
 * \param pszValue  Pointer to a string in which the name's value is placed.
 * 
 * \return SCPE_OK for a successful query, value copied into pszValue; SCPE_ARG when
 * pszValue is NULL, no such name or value isn't a string type.
 */
t_stat queryWindowsRegistry(_In_ HKEY hKey, _In_ LPSTR szName, _Out_ LPSTR *pszValue)
{
    if (pszValue == NULL) {
        return SCPE_ARG;
    }

    DWORD dwValueType = REG_NONE, dwSize = 0;
    DWORD dwResult = RegQueryValueEx(hKey, szName, NULL, &dwValueType, NULL, &dwSize);
    if (dwResult != ERROR_SUCCESS) {
        /* No such key... */
        return SCPE_ARG;
    }

    switch (dwValueType) {
        case REG_SZ:
        case REG_EXPAND_SZ:
        {
            /* Read value. */
            LPTSTR szValue = (LPTSTR) malloc(dwSize);
            if (szValue == NULL) {
                return sim_messagef(SCPE_MEM, "%s: malloc(%lu) failed", __FUNCTION__, dwSize);
            }

            dwResult = RegQueryValueEx(hKey, szName, NULL, NULL, (LPBYTE)szValue, &dwSize);
            if (dwResult != ERROR_SUCCESS) {
                free(szValue);
                return sim_messagef(SCPE_IOERR, "%s: reading \"%" PRIsLPSTR "\" registry value failed", __FUNCTION__, szName);
            }

            if (dwValueType == REG_EXPAND_SZ) {
                /* Expand the environment strings. */
                DWORD dwSizeExp = dwSize * 2;
                DWORD dwCountExp = dwSizeExp / sizeof(CHAR) - 1;
                LPTSTR szValueExp = (LPTSTR) malloc(dwSizeExp);
                if (szValueExp == NULL) {
                    free(szValue);
                    return sim_messagef(SCPE_MEM, "%s: malloc(%lu) failed", __FUNCTION__, dwSizeExp);
                }

                DWORD dwCountExpResult = ExpandEnvironmentStrings(szValue, szValueExp, dwCountExp );
                if (dwCountExpResult == 0) {
                    free(szValueExp);
                    free(szValue);
                    return sim_messagef(SCPE_IOERR, "%s: expanding \"%" PRIsLPSTR "\" registry value failed",
                                        __FUNCTION__, szName);
                } else if (dwCountExpResult <= dwCountExp) {
                    /* The buffer was big enough. */
                    free(szValue);
                    *pszValue = szValueExp;
                    return SCPE_OK;
                } else {
                    /* Retry with a bigger buffer. */
                    free(szValueExp);
                    /* Note: ANSI version requires one extra char. */
                    dwSizeExp = (dwCountExpResult + 1) * sizeof(char);
                    dwCountExp = dwCountExpResult;
                    szValueExp = (LPSTR) malloc(dwSizeExp);
                    if (szValueExp == NULL) {
                        free(szValue);
                        return sim_messagef(SCPE_MEM, "%s: malloc(%lu) failed", __FUNCTION__, dwSizeExp);
                    }

                    dwCountExpResult = ExpandEnvironmentStrings(szValue, szValueExp, dwCountExp);
                    free(szValue);
                    *pszValue = szValueExp;
                    return SCPE_OK;
                }
            } else {
                *pszValue = szValue;
                return SCPE_OK;
            }
        }

        default:
            return sim_messagef(SCPE_ARG, "%s: \"%" PRIsLPSTR "\" registry value is not string (type %lu)",
                                __FUNCTION__, szName, dwValueType);
    }
}

/*!
 * Windows GUID comparison function. Searches for the leading '{' in each name to find
 * the start of the GUID, then compares the two GUIDs until either they don't match or
 * the trailing '}' is found. If the trailing '}' is found, then the GUIDs are identical.
 *
 * \return 0 if the GUIDs match, otherwise, the GUIDs don't match or are improperly formatted
 * (not enclosed by braces.)
 */
static int compare_guid(const char *szPcapName, const LPSTR szIfName)
{
    LPSTR pc, ic;

    /* Skip to the "{" leader in the pcap GUID: */
    pc = strchr(szPcapName, '{');
    if (pc == NULL)
        return -1;

    ++pc;

    /* Skip to the leader in the interface's GUID. */
    ic = strchr(szIfName, '{');
    if (ic == NULL)
        return 1;

    ++ic;

    /* Match the rest of the string */
    for (/* empty */; *pc != 0 && *ic != 0 && *pc != '}' && *ic != '}'; ++pc, ++ic) {
        if (*pc != *ic)
            return *ic - *pc;
    }

    return (*pc != *ic) || (*pc != '}');
}

/* Look up an adapter's user-defined description in the Windows registry.
 *
 * char *dev_guid: The device's brace-enclosed GUID string, e.g., "{...}"
 * char **description: The returned description, must be free()-ed by the caller.
 */
t_stat windows_eth_dev_description(const char *dev_guid, char **description)
{
    /* Assume something other than SCPE_OK. */
    t_stat retval = SCPE_ARG;

    if (szDevClassNetId == NULL) {
        StringFromIID((REFIID) &GUID_DEVCLASS_NET, &szDevClassNetId);
    }

    char adapterKey[ADAPTER_REGKEY_PATH_MAX];

    /* Convert the list's GUID to a wide string so we can reuse the template. */
    const int lenDevGUID = (int) ((strlen(dev_guid) + 1) * sizeof(wchar_t));
    LPOLESTR wszDeDevGUID = (LPOLESTR) malloc(lenDevGUID);

    MultiByteToWideChar(CP_UTF8, 0, dev_guid, -1, wszDeDevGUID, lenDevGUID);
    int nlen = snprintf(adapterKey, ADAPTER_REGKEY_PATH_MAX, szAdapterRegKeyPathTemplate, szDevClassNetId, wszDeDevGUID);
    free(wszDeDevGUID);

    if (nlen <= ADAPTER_REGKEY_PATH_MAX) {
        /* These registry keys don't seem to exist for all devices, so we simply ignore errors. */
        HKEY hKey = NULL;
        DWORD dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, adapterKey, 0, KEY_READ, &hKey);

        if (dwResult == ERROR_SUCCESS) {
            if (queryWindowsRegistry(hKey, "DriverDesc", description) == SCPE_OK) {
                retval = SCPE_OK;
            }
        
            RegCloseKey(hKey);
        }
    } else {
        sim_printf("%s: regkey template overflow\n", __FUNCTION__);
    }

    return retval;
}
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
/* The previous pcap_mac_if_win32 implementation use the NPCAP **internal** packet API to
 * acquire the adapter's MAC address and used dynamic library loading from packet.dll to
 * call these internal functions. This approach is inherently unsafe because, as the NPCAP
 * developers repeatedly point out, the packet API is internal and subject to unexpected
 * changes.
 * 
 * It's very likely that packet.dll's API did change because the reimplemented code below
 * does not result in mystery pointer and malloc() problems when SIMH exits.
 */

/* Look up the AdapterName's MAC address.
 * 
 * Returns:
 *  0: Successful
 * -1: Did not find the adapter's MAC address.
 */
int pcap_mac_if_win32(const char *AdapterName, unsigned char MACAddress[6])
{
    /* Assume failure until proven otherwise. */
    int retval = -1;
    const IP_ADAPTER_ADDRESSES *adapter = windowsNetworkAdapterList();

    for (adapter = windowsNetworkAdapterList();
         retval < 0 && adapter != (IP_ADAPTER_ADDRESSES *) NULL;
         adapter = adapter->Next) {
        if (!compare_guid(AdapterName, adapter->AdapterName)) {
            if (adapter->PhysicalAddressLength == 6) {
                memcpy(MACAddress, adapter->PhysicalAddress, 6);
                retval = 0;
            }

            /* Some adapters don't have MAC addresses and we matched the GUID,
             * so no point in continuing the search. */
            break;
        }
    }

    return retval;
}
#endif  /* defined(_WIN32) || defined(__CYGWIN__) */

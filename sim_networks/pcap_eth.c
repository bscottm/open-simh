/*=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~
 * PCAP-based simulated Ethernet implementation:
 *=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~*/

#include "sim_ether.h"
#include "sim_networks/sim_networks.h"
#include "sim_networks/net_support.h"

#ifdef SIM_HAVE_DLOPEN
#include <dlfcn.h>
#endif

#if defined(HAVE_PCAP_NETWORK)
  /* Forward decl's... */
  static int pcap_reader(ETH_DEV *eth_dev, int ms_timeout);
  static int pcap_writer(ETH_DEV *eth_dev, ETH_PACK *packet);
  
  /* PCAP API functions: */
  const eth_apifuncs_t pcap_api_funcs = {
    .reader = pcap_reader,
    .writer = pcap_writer,
    .reader_shutdown = default_reader_shutdown,
    .writer_shutdown = default_writer_shutdown
  };

  /* Assert more control over how the pcap state is constructed, intead of calling
    * pcap_open_live(). It isn't "overkill" or "pedantic" -- if, for some reason,
    * we need to call pcap_set_rfmon() for wireless monitor mode, we can insert it
    * easily here.
    */
  t_stat sim_pcap_open(eth_apidata_t *pcap_api, const char *dev_name, int bufsz, char *errbuf)
  {

      pcap_t *pcap = pcap_create(dev_name, errbuf);
      const char *fn_name = NULL;

      if (pcap != NULL) {
          /* Works for both wired and wireless. */
          if (fn_name = "pcap_set_snaplen", pcap_set_snaplen(pcap, bufsz) < 0)
              goto openerr;
          if (fn_name = "pcap_set_promisc", pcap_set_promisc(pcap, ETH_PROMISC) < 0)
              goto openerr;
          if (fn_name = "pcap_set_timeout", pcap_set_timeout(pcap, PCAP_READ_TIMEOUT) < 0)
              goto openerr;
          if (fn_name = "pcap_activate", pcap_activate(pcap) < 0)
              goto openerr;

          pcap_api->pcap.handle = pcap;
      } else {
          pcap_api->pcap.handle = NULL;
          return sim_messagef(SCPE_OPENERR, "pcap_create() error: %s", errbuf);
      }

      return SCPE_OK;

  openerr:
      pcap_close(pcap);
      return sim_messagef(SCPE_OPENERR, "%s() error: %s", fn_name, pcap_geterr(pcap));
  }

  void sim_pcap_close(eth_apidata_t *pcap_api)
  {
      if (pcap_api != NULL && pcap_api->pcap.handle != NULL) {
        pcap_close(pcap_api->pcap.handle);
      }
  }

  void pcap_callback(u_char *eth_opaque, const struct pcap_pkthdr *header, const u_char *data)
  {
      sim_eth_callback((ETH_DEV *) eth_opaque, header->len, header->caplen, data);
  }

  static int pcap_reader(ETH_DEV *eth_dev, int ms_timeout)
  {
      /* For the non-USE_READER_THREAD path, need a value that calls pcap_dispatch(). */
      int retval = 1;

#  if defined(USE_READER_THREAD)
#    if (!defined(_WIN32) && !defined(_WIN64)) || defined(MUST_DO_SELECT)
        retval = netsupport_poll_socket(pcap_get_selectable_fd(eth_dev->api_data.pcap.handle), ms_timeout);
#    else
       /* Windows path: */
        switch (WaitForSingleObject (pcap_getevent(eth_dev->api_data.pcap.handle), ms_timeout)) {
        case WAIT_OBJECT_0:
            retval = 1;
            break;
        case WAIT_TIMEOUT:
            retval = 0;
            break;
        default:
            retval = -1;
            break;
        }
#    endif
#  endif

      if (retval > 0)
          pcap_dispatch (eth_dev->api_data.pcap.handle, -1, &pcap_callback, (u_char*) eth_dev);

      return retval;
    }

  static int pcap_writer(ETH_DEV *eth_dev, ETH_PACK *packet)
  {
      int retval = pcap_sendpacket(eth_dev->api_data.pcap.handle, (u_char *) packet->msg, packet->len);

      if (retval < 0) {
          sim_messagef(SCPE_IOERR, "pcap_sendpacket: %s\n", pcap_geterr(eth_dev->api_data.pcap.handle));
      }

      return (retval < 0);
  }

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Dynamically loaded NPCAP/libpcap:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

#if defined(USE_SHARED) && (defined(_WIN32) || defined(_WIN64) || defined(SIM_HAVE_DLOPEN))
/* Dynamic DLL loading technique and modified source comes from
   Etherial/WireShark capture_pcap.c */

/* Dynamic DLL load variables */
#ifdef _WIN32
static HINSTANCE hLib = NULL;               /* handle to DLL */
#else
static void *hLib = 0;                      /* handle to Library */
#endif

static int lib_loaded = 0;                  /* 0=not loaded, 1=loaded, 2=library load failed, 3=Func load failed */

#define __STR_QUOTE(tok) #tok
#define __STR(tok) __STR_QUOTE(tok)

static const char* lib_name =
#if defined(_WIN32) || defined(__CYGWIN__)
                          "wpcap.dll";
#elif defined(__APPLE__)
                          "/usr/lib/libpcap.A.dylib";
#else
                          "libpcap." __STR(SIM_HAVE_DLOPEN);
#endif

static char no_pcap[PCAP_ERRBUF_SIZE] =
#if defined(_WIN32) || defined(__CYGWIN__)
    "wpcap.dll failed to load, install Npcap or WinPcap 4.1.3 to use pcap networking";
#elif defined(__APPLE__)
    "/usr/lib/libpcap.A.dylib failed to load, install libpcap to use pcap networking";
#else
    "libpcap." __STR(SIM_HAVE_DLOPEN) " failed to load, install libpcap to use pcap networking";
#endif
#undef __STR
#undef __STR_QUOTE

/* define pointers to pcap functions needed */
static int     (*p_pcap_activate)(pcap_t *);
static pcap_t *(*p_pcap_create)(const char *, char *);
static void    (*p_pcap_close) (pcap_t *);
static int     (*p_pcap_compile) (pcap_t *, struct bpf_program *, const char *, int, bpf_u_int32);
static int     (*p_pcap_datalink) (pcap_t *);
static int     (*p_pcap_dispatch) (pcap_t *, int, pcap_handler, u_char *);
static int     (*p_pcap_findalldevs) (pcap_if_t **, char *);
static void    (*p_pcap_freealldevs) (pcap_if_t *);
static void    (*p_pcap_freecode) (struct bpf_program *);
static char*   (*p_pcap_geterr) (pcap_t *);
static int     (*p_pcap_lookupnet) (const char *, bpf_u_int32 *, bpf_u_int32 *, char *);
static pcap_t* (*p_pcap_open_live) (const char *, int, int, int, char *);
#ifdef _WIN32
static int     (*p_pcap_setmintocopy) (pcap_t* handle, int);
static HANDLE  (*p_pcap_getevent) (pcap_t *);
static pcap_send_queue* (*p_pcap_sendqueue_alloc)(u_int memsize);
static void    (*p_pcap_sendqueue_destroy)(pcap_send_queue* queue);
static int     (*p_pcap_sendqueue_queue)(pcap_send_queue* queue, const struct pcap_pkthdr *pkt_header, const u_char *pkt_data);
static u_int   (*p_pcap_sendqueue_transmit)(pcap_t *p, pcap_send_queue* queue, int sync);
#else
#ifdef MUST_DO_SELECT
static int     (*p_pcap_get_selectable_fd) (pcap_t *);
#endif
static int     (*p_pcap_fileno) (pcap_t *);
#endif
static int     (*p_pcap_sendpacket) (pcap_t* handle, const u_char* msg, int len);
static int     (*p_pcap_setfilter) (pcap_t *, struct bpf_program *);
static int     (*p_pcap_setnonblock)(pcap_t* a, int nonblock, char *errbuf);
static int     (*p_pcap_set_snaplen)(pcap_t *, int);
static int     (*p_pcap_set_promisc)(pcap_t *, int);
static int     (*p_pcap_set_timeout)(pcap_t *, int);
static char   *(*p_pcap_lib_version) (void);

/* load function pointer from DLL */
typedef int (*_func)();

static void load_function(const char* function, _func* func_ptr) {
#ifdef _WIN32
    *func_ptr = (_func) GetProcAddress(hLib, function);
#else
    *func_ptr = (_func) dlsym(hLib, function);
#endif
    if (*func_ptr == NULL) {
      sim_printf ("Eth: Failed to find function '%s' in %s\n", function, lib_name);
      lib_loaded = 3;
    }
}

/* load wpcap.dll as required */
int load_pcap(void) {
  switch(lib_loaded) {
    case 0:                  /* not loaded */
            /* attempt to load DLL */
#ifdef _WIN32
      {
        BOOL(WINAPI *p_SetDllDirectory)(LPCTSTR);
        UINT(WINAPI *p_GetSystemDirectory)(LPTSTR lpBuffer, UINT uSize);
        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");

        p_SetDllDirectory = (BOOL(WINAPI *)(LPCTSTR)) GetProcAddress(kernel32, "SetDllDirectoryA");
        p_GetSystemDirectory = (UINT(WINAPI *)(LPTSTR, UINT)) GetProcAddress(kernel32, "GetSystemDirectoryA");

        if (p_SetDllDirectory && p_GetSystemDirectory) {
          char npcap_path[512] = "";

          if (p_GetSystemDirectory (npcap_path, sizeof(npcap_path) - 7))
            strlcat (npcap_path, "\\Npcap", sizeof(npcap_path));
          if (p_SetDllDirectory(npcap_path))
            hLib = LoadLibraryA(lib_name);
          p_SetDllDirectory (NULL);
          }
        if (hLib == NULL)
          hLib = LoadLibraryA(lib_name);
        }
#else
      hLib = dlopen(lib_name, RTLD_NOW);
#endif
      if (hLib == 0) {
        /* failed to load DLL */
        lib_loaded = 2;
        break;
      } else {
        /* library loaded OK */
        lib_loaded = 1;

        /* load required functions; sets dll_load=3 on error */
        load_function("pcap_activate",     (_func *) &p_pcap_activate);
        load_function("pcap_create",       (_func *) &p_pcap_create);
        load_function("pcap_close",        (_func *) &p_pcap_close);
        load_function("pcap_compile",      (_func *) &p_pcap_compile);
        load_function("pcap_datalink",     (_func *) &p_pcap_datalink);
        load_function("pcap_dispatch",     (_func *) &p_pcap_dispatch);
        load_function("pcap_findalldevs",  (_func *) &p_pcap_findalldevs);
        load_function("pcap_freealldevs",  (_func *) &p_pcap_freealldevs);
        load_function("pcap_freecode",     (_func *) &p_pcap_freecode);
        load_function("pcap_geterr",       (_func *) &p_pcap_geterr);
        load_function("pcap_lookupnet",    (_func *) &p_pcap_lookupnet);
        load_function("pcap_open_live",    (_func *) &p_pcap_open_live);
#ifdef _WIN32
        load_function("pcap_setmintocopy", (_func *) &p_pcap_setmintocopy);
        load_function("pcap_getevent",     (_func *) &p_pcap_getevent);
        load_function("pcap_sendqueue_alloc",    (_func *) &p_pcap_sendqueue_alloc);
        load_function("pcap_sendqueue_queue",    (_func *) &p_pcap_sendqueue_queue);
        load_function("pcap_sendqueue_transmit", (_func *) &p_pcap_sendqueue_transmit);
        load_function("pcap_sendqueue_destroy",  (_func *) &p_pcap_sendqueue_destroy);
#else
#ifdef MUST_DO_SELECT
        load_function("pcap_get_selectable_fd",     (_func *) &p_pcap_get_selectable_fd);
#endif
        load_function("pcap_fileno",       (_func *) &p_pcap_fileno);
#endif
        load_function("pcap_sendpacket",   (_func *) &p_pcap_sendpacket);
        load_function("pcap_setfilter",    (_func *) &p_pcap_setfilter);
        load_function("pcap_setnonblock",  (_func *) &p_pcap_setnonblock);
        load_function("pcap_set_promisc",  (_func *) &p_pcap_set_promisc);
        load_function("pcap_set_snaplen",  (_func *) &p_pcap_set_snaplen);
        load_function("pcap_set_timeout",  (_func *) &p_pcap_set_timeout);
        load_function("pcap_lib_version",  (_func *) &p_pcap_lib_version);
        }
      break;
    default:                /* loaded or failed */
      break;
  }
  return (lib_loaded == 1) ? 1 : 0;
}

/* define functions with dynamic revectoring */
int pcap_activate(pcap_t *pcap)
{
  if (load_pcap() != 0) {
    return p_pcap_activate(pcap);
  } else
    return -1;
}

pcap_t *pcap_create(const char *dev, char *errbuf)
{
  if (load_pcap() != 0) {
    return p_pcap_create(dev, errbuf);
  } else
    return NULL;
}

void pcap_close(pcap_t* a) {
  if (load_pcap() != 0) {
    p_pcap_close(a);
  }
}

/* Some platforms's pcap.h have an ancient declaration of pcap_compile which doesn't have a const in the bpf string argument */
#if !defined (BPF_CONST_STRING)
int pcap_compile(pcap_t* a, struct bpf_program* b, char* c, int d, bpf_u_int32 e) {
#else
int pcap_compile(pcap_t* a, struct bpf_program* b, const char* c, int d, bpf_u_int32 e) {
#endif
  if (load_pcap() != 0) {
    return p_pcap_compile(a, b, c, d, e);
  } else {
    return 0;
  }
}

const char *pcap_lib_version(void) {
  static char buf[256];

  if ((load_pcap() != 0) && (p_pcap_lib_version != NULL)) {
    return p_pcap_lib_version();
  } else {
    sprintf (buf, "%s not installed",
#if defined(_WIN32)
        "npcap or winpcap"
#else
        "libpcap"
#endif
        );
    return buf;
  }
}

int pcap_datalink(pcap_t* a) {
  if (load_pcap() != 0) {
    return p_pcap_datalink(a);
  } else {
    return 0;
  }
}

int pcap_dispatch(pcap_t* a, int b, pcap_handler c, u_char* d) {
  if (load_pcap() != 0) {
    return p_pcap_dispatch(a, b, c, d);
  } else {
    return 0;
  }
}

int pcap_findalldevs(pcap_if_t** a, char* b) {
  if (load_pcap() != 0) {
    return p_pcap_findalldevs(a, b);
  } else {
    *a = 0;
    strcpy(b, no_pcap);
    no_pcap[0] = '\0';
    return -1;
  }
}

void pcap_freealldevs(pcap_if_t* a) {
  if (load_pcap() != 0) {
    p_pcap_freealldevs(a);
  }
}

void pcap_freecode(struct bpf_program* a) {
  if (load_pcap() != 0) {
    p_pcap_freecode(a);
  }
}

char* pcap_geterr(pcap_t* a) {
  if (load_pcap() != 0) {
    return p_pcap_geterr(a);
  } else {
    return (char*) "";
  }
}

int pcap_lookupnet(const char* a, bpf_u_int32* b, bpf_u_int32* c, char* d) {
  if (load_pcap() != 0) {
    return p_pcap_lookupnet(a, b, c, d);
  } else {
    return 0;
  }
}

pcap_t* pcap_open_live(const char* a, int b, int c, int d, char* e) {
  if (load_pcap() != 0) {
    return p_pcap_open_live(a, b, c, d, e);
  } else {
    return (pcap_t*) 0;
  }
}

#ifdef _WIN32
int pcap_setmintocopy(pcap_t* a, int b) {
  if (load_pcap() != 0) {
    return p_pcap_setmintocopy(a, b);
  } else {
    return -1;
  }
}

HANDLE pcap_getevent(pcap_t* a) {
  if (load_pcap() != 0) {
    return p_pcap_getevent(a);
  } else {
    return (HANDLE) 0;
  }
}

static pcap_send_queue*pcap_sendqueue_alloc(u_int memsize)
{
  if (load_pcap() != 0) {
    return p_pcap_sendqueue_alloc(memsize);
  } else
    return NULL;
}

static void pcap_sendqueue_destroy(pcap_send_queue* queue)
{
  if (load_pcap() != 0) {
    p_pcap_sendqueue_destroy(queue);
  }
}

static int pcap_sendqueue_queue(pcap_send_queue* queue, const struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
  if (load_pcap() != 0)
    return p_pcap_sendqueue_queue(queue, pkt_header, pkt_data);
  else
    return -1;
}

static u_int pcap_sendqueue_transmit(pcap_t *p, pcap_send_queue* queue, int sync)
{
  if (load_pcap() != 0)
    return p_pcap_sendqueue_transmit(p, queue, sync);
  else
    return -1;
}
#else
#ifdef MUST_DO_SELECT
int pcap_get_selectable_fd(pcap_t* a) {
  if (load_pcap() != 0) {
    return p_pcap_get_selectable_fd(a);
  } else {
    return 0;
  }
}
#endif

int pcap_fileno(pcap_t * a) {
  if (load_pcap() != 0) {
    return p_pcap_fileno(a);
  } else {
    return 0;
  }
}
#endif

int pcap_sendpacket(pcap_t* a, const u_char* b, int c) {
  if (load_pcap() != 0) {
    return p_pcap_sendpacket(a, b, c);
  } else {
    return 0;
  }
}

int pcap_setfilter(pcap_t* a, struct bpf_program* b) {
  if (load_pcap() != 0) {
    return p_pcap_setfilter(a, b);
  } else {
    return 0;
  }
}

int pcap_setnonblock(pcap_t* a, int nonblock, char *errbuf) {
  if (load_pcap() != 0) {
    return p_pcap_setnonblock(a, nonblock, errbuf);
  } else {
    return 0;
  }
}

int pcap_set_promisc(pcap_t *pcap, int promisc)
{
  if (load_pcap() != 0) {
    return p_pcap_set_promisc(pcap, promisc);
  } else
    return 0;
}

int pcap_set_snaplen(pcap_t *pcap, int len)
{
  if (load_pcap() != 0) {
    return p_pcap_set_snaplen(pcap, len);
  } else
    return 0;
}

int pcap_set_timeout(pcap_t *pcap, int tmo)
{
  if (load_pcap() != 0) {
    return p_pcap_set_timeout(pcap, tmo);
  } else
    return 0;
}
#endif /* defined(USE_SHARED) && (defined(_WIN32) || defined(SIM_HAVE_DLOPEN)) */

#endif /* HAVE_PCAP_NETWORK */

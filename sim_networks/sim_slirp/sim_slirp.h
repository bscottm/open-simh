#if !defined(SIM_SLIRP_H)
#  if defined(HAVE_SLIRP_NETWORK)

#  include "sim_defs.h"
#  include "libslirp.h"

    /* Simulator's libslirp state (see sim_slirp_network.h for the complete structure
    * definition): */
    typedef struct sim_slirp SimSlirpNetwork;

    t_stat sim_slirp_attach_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
    void sim_slirp_show (SimSlirpNetwork *slirp, FILE *st);
#  endif /* HAVE_SLIRP_NETWORK */


#define SIM_SLIRP_H
#endif

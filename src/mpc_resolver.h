#ifndef __RESOLVER_H_INCLUDED__
#define __RESOLVER_H_INCLUDED__

#include <netdb.h>
#include "ares.h"


#define AE_RESOLVER_OK      ARES_SUCCESS


typedef void (*ae_gethostbyname_cb)(ae_event_loop *el, int status,
        struct hostent *host, void *arg);

/*
 * By 'server' parameter, you can specify DNS server instead of 
 * using /etc/resolv.conf. The string format is CSV. You can NOT
 * add whitespace between two servers.
 */
void ae_gethostbyname(ae_event_loop *el, ae_gethostbyname_cb callback, 
    const char *name, int family, void *arg, const char *server);


#endif /* __RESOLVER_H_INCLUDED__ */

#include <stdlib.h>
#include <assert.h>
#include "ae.h"

typedef struct ae_resolver {
    ae_event_loop           *el;
    ae_gethostbyname_cb      callback;
    void                    *arg;
    int                      mask;
    ares_channel             channel;
    int                      channel_over;
    ares_socket_t            socks[ARES_GETSOCK_MAXNUM];
} ae_resolver_t;


static void
ae_resolver_callback(void *arg, int status, int timeouts, struct hostent *host)
{
    ae_resolver_t  *resolver = (ae_resolver_t *)arg;
    size_t          i;

    if (status == ARES_EDESTRUCTION) {
        /* do nothing here */
        return;
    }

    if (status != ARES_SUCCESS) {
        resolver->callback(resolver->el, status, NULL, 
                           resolver->arg);
    } else {
        resolver->callback(resolver->el, AE_RESOLVER_OK, host, 
                           resolver->arg);
    }

    resolver->channel_over = 1;
}


static void
ae_resolver_process(struct ae_event_loop *el, int fd, void *arg, int mask)
{
    ae_resolver_t *resolver = (ae_resolver_t *)arg;
    size_t         i;
    
    if (mask & AE_READABLE) {
        ares_process_fd(resolver->channel, fd, ARES_SOCKET_BAD);
    }

    if (mask & AE_WRITABLE) {
        ares_process_fd(resolver->channel, ARES_SOCKET_BAD, fd);
    }

    if (resolver->channel_over) {
        for (i = 0; i < ARES_GETSOCK_MAXNUM; ++i) {
            if (ARES_GETSOCK_READABLE(resolver->mask, i)) {
                ae_delete_file_event(resolver->el, resolver->socks[i], 
                                     AE_READABLE);
            }

            if (ARES_GETSOCK_WRITABLE(resolver->mask, i)) {
                ae_delete_file_event(resolver->el, resolver->socks[i], 
                                     AE_WRITABLE);
            }
        }

        ares_destroy(resolver->channel);
        free(resolver);
    }
}


void
ae_gethostbyname(ae_event_loop *el, ae_gethostbyname_cb callback, 
    const char *name, int family, void *arg, const char *server)
{
    ae_resolver_t   *resolver;
    int              mask;
    size_t           i;
    int              events;

    resolver = (ae_resolver_t *)calloc(sizeof(ae_resolver_t), 1);
    assert(resolver);
    assert(ares_init(&resolver->channel) == ARES_SUCCESS);

    if (server) {
        ares_set_servers_csv(resolver->channel, server);
    }

    /*
     * set by calloc():
     *   resolver->channel_over = 0;
     */

    resolver->el = el;
    resolver->callback = callback;
    resolver->arg = arg;

    ares_gethostbyname(resolver->channel, name, family, ae_resolver_callback, 
                       resolver);

    mask = ares_getsock(resolver->channel, resolver->socks, 
                        ARES_GETSOCK_MAXNUM);
    resolver->mask = mask;

    for (i = 0; i < ARES_GETSOCK_MAXNUM; ++i) {
        if (ARES_GETSOCK_READABLE(mask, i)) {
            ae_create_file_event(el, resolver->socks[i], AE_READABLE, 
                                 ae_resolver_process, resolver);
        }

        if (ARES_GETSOCK_WRITABLE(mask, i)) {
            ae_create_file_event(el, resolver->socks[i], AE_WRITABLE, 
                                 ae_resolver_process, resolver);
        }
    }
}

/* 
 * gcc ae.c  ae_resolver.c  -lcares -g -D WITH_AE_RESOLVER \
 *   -D AE_RESOLVER_TEST_MAIN
 */ 
#ifdef AE_RESOLVER_TEST_MAIN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int over_count = 0;

static void 
print_ip(ae_event_loop *el, int status,
    struct hostent *host, void *arg)
{
    if (status == AE_RESOLVER_OK) {
        uint32_t addr = *(in_addr_t *)host->h_addr;
        printf("%s: %u.%u.%u.%u ", (const char *)arg, 
               addr & 0xFF, (addr & 0xFF00) >> 8,
               (addr & 0xFF0000) >> 16, (addr & 0xFF000000) >> 24);
    } else {
        printf("%s: %d, %s ", (const char *)arg, status, 
               ares_strerror(status));
    }

    free(arg);

    printf("%d\n", __sync_add_and_fetch(&over_count, 1));
}

static void 
add_all(ae_event_loop *el, const char *filename)
{
    int i = 0, j = 0, n;
    char buf[1024];
    FILE *fp = fopen(filename, "r");

    while (fgets(buf, 1024, fp)) {
        n = strlen(buf);
        if (buf[n - 1] == '\n')  {
            buf[n - 1] = '\0';
        }

        if (buf[0] == '#') {
            continue;
        }

        ae_gethostbyname(el, print_ip, buf, AF_INET, (void *)strdup(buf),
                         "114.114.114.114,8.8.8.8");
    }
    fclose(fp);
}


int 
main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    ares_library_init(ARES_LIB_INIT_ALL);

    ae_event_loop *el = ae_create_event_loop();
    add_all(el, argv[1]);
    ae_main(el);
    exit(0);
}

#endif /* AE_RESOLVER_TEST_MAIN */

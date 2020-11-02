/*
 * WPcap.dll Proxy.
 *
 * Copyright 2011, 2014 André Hentschel
 * Copyright 2019 Conor McCarthy for Codeweavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <pcap/pcap.h>

/* pcap.h might define those: */
#undef SOCKET
#undef INVALID_SOCKET

#define USE_WS_PREFIX
#include <stdarg.h>
#ifndef _VA_LIST_T /* Clang's stdarg.h guards with _VA_LIST, while Xcode's uses _VA_LIST_T */
#define _VA_LIST_T
#endif
#include "winsock2.h"
#include "windef.h"
#include "winbase.h"
#include "wine/heap.h"
#include "wine/static_strings.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wpcap);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

#ifndef PCAP_SRC_FILE_STRING
#define PCAP_SRC_FILE_STRING    "file://"
#endif
#ifndef PCAP_SRC_FILE
#define PCAP_SRC_FILE           2
#endif
#ifndef PCAP_SRC_IF_STRING
#define PCAP_SRC_IF_STRING      "rpcap://"
#endif
#ifndef PCAP_SRC_IFLOCAL
#define PCAP_SRC_IFLOCAL        3
#endif

DECLARE_STATIC_STRINGS(pcap_strings);

static inline WCHAR *heap_strdupAtoW(const char *str)
{
    LPWSTR ret = NULL;

    if(str) {
        DWORD len;

        len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
        ret = heap_alloc(len*sizeof(WCHAR));
        if(ret)
            MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    }

    return ret;
}

#ifdef __i386_on_x86_64__

typedef struct
{
    pcap_t *p;
    unsigned char *pkt_data;
    size_t pkt_alloc_size;
    struct pcap_pkthdr pkt_header;
    char error[PCAP_ERRBUF_SIZE + 1];
} wine_pcap_t;

typedef struct
{
	u_int bf_len;
	struct bpf_insn * WIN32PTR bf_insns;
} wine_bpf_program;

struct wine_pcap_addr
{
	struct wine_pcap_addr *next;
	struct WS_sockaddr * WIN32PTR addr;
	struct WS_sockaddr * WIN32PTR netmask;
	struct WS_sockaddr * WIN32PTR broadaddr;
	struct WS_sockaddr * WIN32PTR dstaddr;
    struct WS_sockaddr objects[4];
};

struct wine_pcap_if
{
	struct wine_pcap_if *next;
	char *name;
	char *description;
	struct wine_pcap_addr *addresses;
	bpf_u_int32 flags;
};

typedef struct wine_pcap_if wine_pcap_if_t;

static int pcap_program_conv(wine_bpf_program *dest, struct bpf_program *src)
{
    int ret = 0;

    dest->bf_insns = heap_alloc(src->bf_len);
    if (!dest->bf_insns)
    {
        ERR("Failed to allocate program local buffer.\n");
        dest->bf_len = 0;
        ret = -1;
    }
    else
    {
        dest->bf_len = src->bf_len;
        memcpy(dest->bf_insns, src->bf_insns, src->bf_len);
    }
    pcap_freecode(src);
    return ret;
}

static void pcap_free_program_code(wine_bpf_program *program)
{
    if (program)
        heap_free(program->bf_insns);
}

static char *pcap_update_error(wine_pcap_t *wp, char * HOSTPTR err)
{
    memccpy(wp->error, err, 0, sizeof(wp->error) - 1);
    return wp->error;
}

static inline wine_pcap_t *pcap_wrap(pcap_t *p)
{
    wine_pcap_t *wp = NULL;
    if (p)
    {
        wp = heap_alloc_zero(sizeof(wine_pcap_t));
        if (!wp)
            pcap_close(p);
        else
            wp->p = p;
    }
    return wp;
}

static inline pcap_t *pcap_get(wine_pcap_t *wp)
{
    return wp->p;
}

static inline void pcap_unwrap(wine_pcap_t *wp)
{
    if (wp)
    {
        heap_free(wp->pkt_data);
        heap_free(wp);
    }
    /* It may make more sense to do this on DLL unload, but this will do. */
    wine_static_string_free(&pcap_strings);
}

static struct wine_pcap_addr *pcap_dup_address(const struct pcap_addr *address)
{
    struct wine_pcap_addr *ret = NULL;

    if (address && (ret = heap_alloc(sizeof(struct wine_pcap_addr))))
    {
        ret->next = NULL;
        ret->addr = ret->objects;
        memcpy(ret->addr, address->addr, sizeof(struct WS_sockaddr));
        ret->netmask = NULL;
        ret->broadaddr = NULL;
        ret->dstaddr = NULL;
        if (address->netmask)
        {
            ret->netmask = ret->objects + 1;
            memcpy(ret->netmask, address->netmask, sizeof(struct WS_sockaddr));
        }
        if (address->broadaddr)
        {
            ret->broadaddr = ret->objects + 2;
            memcpy(ret->broadaddr, address->broadaddr, sizeof(struct WS_sockaddr));
        }
        if (address->dstaddr)
        {
            ret->dstaddr = ret->objects + 3;
            memcpy(ret->dstaddr, address->dstaddr, sizeof(struct WS_sockaddr));
        }
    }
    return ret;
}

static struct wine_pcap_addr *pcap_dup_addresses(const struct pcap_addr *addresses)
{
    struct wine_pcap_addr *addrs = pcap_dup_address(addresses);
    struct wine_pcap_addr *tail = addrs;
    while (tail && addresses->next)
    {
        addresses = addresses->next;
        tail->next = pcap_dup_address(addresses);
        tail = tail->next;
    }
    return addrs;
}

static wine_pcap_if_t *pcap_dup_device(const pcap_if_t *device)
{
    wine_pcap_if_t *dev = NULL;

    if (device && (dev = heap_alloc(sizeof(wine_pcap_if_t))))
    {
        dev->next = NULL;
        dev->name = heap_strdup(device->name);
        dev->description = heap_strdup(device->description);
        dev->addresses = pcap_dup_addresses(device->addresses);
        dev->flags = device->flags;
    }
    return dev;
}

static int pcap_conv_alldevs(pcap_if_t *alldevs, wine_pcap_if_t **conv)
{
    wine_pcap_if_t *devs = pcap_dup_device(alldevs);
    wine_pcap_if_t *tail = devs;
    while (tail && alldevs->next)
    {
        alldevs = alldevs->next;
        tail->next = pcap_dup_device(alldevs);
        tail = tail->next;
    }
    pcap_freealldevs(alldevs);
    *conv = devs;
    return alldevs && !devs ? -1 : 0;
}

static void pcap_free_addresses(struct wine_pcap_addr *addresses)
{
    while (addresses)
    {
        struct wine_pcap_addr *next = addresses->next;
        heap_free(addresses);
        addresses = next;
    }
}

static void pcap_free_dev_list(wine_pcap_if_t *alldevs)
{
    while (alldevs)
    {
        wine_pcap_if_t *next = alldevs->next;
        heap_free(alldevs->name);
        heap_free(alldevs->description);
        pcap_free_addresses(alldevs->addresses);
        heap_free(alldevs);
        alldevs = next;
    }
}

static int pcap_dup_pkt(wine_pcap_t *wp, const struct pcap_pkthdr *pkt_header, const u_char * HOSTPTR pkt_data,
                        const unsigned char **wine_pkt_data)
{
    *wine_pkt_data = NULL;
    if (wp->pkt_alloc_size < pkt_header->caplen)
    {
        unsigned char *data = heap_realloc(wp->pkt_data, pkt_header->caplen);
        if (!data)
            return -1;
        wp->pkt_data = data;
        wp->pkt_alloc_size = pkt_header->caplen;
    }
    memcpy(wp->pkt_data, pkt_data, pkt_header->caplen);
    *wine_pkt_data = wp->pkt_data;
    return 0;
}

static void pcap_dup_pkt_header(wine_pcap_t *wp, const struct pcap_pkthdr *h, struct pcap_pkthdr * WIN32PTR *pkt_header)
{
    wp->pkt_header = *h;
    *pkt_header = &wp->pkt_header;
}

/* pcap_free_datalinks() is missing from Wine, so this memory will be leaked
 * until that function is added. No bugs have been filed to request it. */
static int pcap_conv_datalinks(int * HOSTPTR src, int count, int **dest)
{
    int *buf = NULL;

    if (count >= 0 && src)
    {
        size_t size = count * sizeof(int);
        buf = heap_alloc(size);
        if (buf)
            memcpy(buf, src, size);
        else
            count = -1;
    }
    *dest = buf;
    return count;
}

#ifndef IFNAMSIZ
#define IFNAMSIZ 256 /* libpcap sets 8192 if undefined but that is excessive */
#endif

#else

typedef pcap_t wine_pcap_t;
typedef pcap_if_t wine_pcap_if_t;
typedef struct bpf_program wine_bpf_program;

static inline int pcap_program_conv(wine_bpf_program *dest, struct bpf_program *src)
{
    *dest = *src;
    return 0;
}

static inline void pcap_free_program_code(wine_bpf_program *program)
{
    pcap_freecode(program);
}

static inline char *pcap_update_error(wine_pcap_t *wp, char * HOSTPTR err)
{
    (void)wp;
    return err;
}

static inline wine_pcap_t *pcap_wrap(pcap_t *p)
{
    return p;
}

static inline pcap_t *pcap_get(wine_pcap_t *wp)
{
    return wp;
}

static inline void pcap_unwrap(wine_pcap_t *wp)
{
    (void)wp;
}

static inline int pcap_conv_alldevs(pcap_if_t *alldevs, wine_pcap_if_t **conv)
{
    *conv = alldevs;
    return 0;
}

static inline void pcap_free_dev_list(wine_pcap_if_t *alldevs)
{
    pcap_freealldevs(alldevs);
}

static inline int pcap_dup_pkt(wine_pcap_t *wp, const struct pcap_pkthdr *pkt_header, const u_char * HOSTPTR pkt_data,
        const unsigned char **wine_pkt_data)
{
    *wine_pkt_data = pkt_data;
    return 0;
}

static inline void pcap_dup_pkt_header(wine_pcap_t *wp, struct pcap_pkthdr *h, struct pcap_pkthdr * WIN32PTR *pkt_header)
{
    (void)wp;
    *pkt_header = h;
}

static inline int pcap_conv_datalinks(int * HOSTPTR src, int count, int **dest)
{
    *dest = src;
    return count;
}

#endif

void CDECL wine_pcap_breakloop(wine_pcap_t *p)
{
    TRACE("(%p)\n", p);
    return pcap_breakloop(pcap_get(p));
}

void CDECL wine_pcap_close(wine_pcap_t *p)
{
    TRACE("(%p)\n", p);
    pcap_close(pcap_get(p));
    pcap_unwrap(p);
}

int CDECL wine_pcap_compile(wine_pcap_t *p, wine_bpf_program *program, const char *buf, int optimize,
                            unsigned int mask)
{
    struct bpf_program prog;
    int ret;
    TRACE("(%p %p %s %i %u)\n", p, program, debugstr_a(buf), optimize, mask);
    if (!(ret = pcap_compile(pcap_get(p), &prog, buf, optimize, mask)))
        ret = pcap_program_conv(program, &prog);
    return ret;
}

int CDECL wine_pcap_datalink(wine_pcap_t *p)
{
    TRACE("(%p)\n", p);
    return pcap_datalink(pcap_get(p));
}

int CDECL wine_pcap_datalink_name_to_val(const char *name)
{
    TRACE("(%s)\n", debugstr_a(name));
    return pcap_datalink_name_to_val(name);
}

const char* CDECL wine_pcap_datalink_val_to_description(int dlt)
{
    TRACE("(%i)\n", dlt);
    return wine_static_string_add(&pcap_strings, pcap_datalink_val_to_description(dlt));
}

const char* CDECL wine_pcap_datalink_val_to_name(int dlt)
{
    TRACE("(%i)\n", dlt);
    return wine_static_string_add(&pcap_strings, pcap_datalink_val_to_name(dlt));
}

typedef struct
{
    void (CALLBACK *pfn_cb)(u_char *, const struct pcap_pkthdr * WIN32PTR, const u_char *);
    void *user_data;
    wine_pcap_t *wp;
} PCAP_HANDLER_CALLBACK;

static void pcap_handler_callback(u_char * HOSTPTR user_data, const struct pcap_pkthdr *h, const u_char * HOSTPTR p)
{
    const struct pcap_pkthdr local_h = *h;
    PCAP_HANDLER_CALLBACK *pcb;
    const unsigned char *buf;

    TRACE("(%p %p %p)\n", user_data, h, p);
    pcb = ADDRSPACECAST(PCAP_HANDLER_CALLBACK*, user_data);
    if (!pcap_dup_pkt(pcb->wp, h, p, &buf))
        pcb->pfn_cb(pcb->user_data, &local_h, buf);
    TRACE("Callback COMPLETED\n");
}

int CDECL wine_pcap_dispatch(wine_pcap_t *p, int cnt,
                             void (CALLBACK *callback)(u_char *, const struct pcap_pkthdr * WIN32PTR, const u_char *),
                             unsigned char *user)
{
    TRACE("(%p %i %p %p)\n", p, cnt, callback, user);

    if (callback)
    {
        PCAP_HANDLER_CALLBACK pcb;
        pcb.pfn_cb = callback;
        pcb.user_data = user;
        pcb.wp = p;
        return pcap_dispatch(pcap_get(p), cnt, pcap_handler_callback, (unsigned char *)&pcb);
    }

    return pcap_dispatch(pcap_get(p), cnt, NULL, user);
}

int CDECL wine_pcap_findalldevs(wine_pcap_if_t **alldevsp, char *errbuf)
{
    pcap_if_t *alldevs = NULL;
    int ret;

    TRACE("(%p %p)\n", alldevsp, errbuf);
    ret = pcap_findalldevs(&alldevs, errbuf);
    if (!alldevs)
        ERR_(winediag)("Failed to access raw network (pcap), this requires special permissions.\n");
    if (!ret)
        ret = pcap_conv_alldevs(alldevs, alldevsp);
    else
        *alldevsp = NULL;
    return ret;
}

int CDECL wine_pcap_findalldevs_ex(char *source, void *auth, wine_pcap_if_t **alldevs, char *errbuf)
{
    FIXME("(%s %p %p %p): partial stub\n", debugstr_a(source), auth, alldevs, errbuf);
    return wine_pcap_findalldevs(alldevs, errbuf);
}

void CDECL wine_pcap_freealldevs(wine_pcap_if_t *alldevs)
{
    TRACE("(%p)\n", alldevs);
    pcap_free_dev_list(alldevs);
}

void CDECL wine_pcap_freecode(wine_bpf_program *fp)
{
    TRACE("(%p)\n", fp);
    pcap_free_program_code(fp);
}

typedef struct _AirpcapHandle *PAirpcapHandle;
PAirpcapHandle CDECL wine_pcap_get_airpcap_handle(wine_pcap_t *p)
{
    TRACE("(%p)\n", p);
    return NULL;
}

char* CDECL wine_pcap_geterr(wine_pcap_t *p)
{
    TRACE("(%p)\n", p);
    return pcap_update_error(p, pcap_geterr(pcap_get(p)));
}

int CDECL wine_pcap_getnonblock(wine_pcap_t *p, char *errbuf)
{
    TRACE("(%p %p)\n", p, errbuf);
    return pcap_getnonblock(pcap_get(p), errbuf);
}

const char* CDECL wine_pcap_lib_version(void)
{
#ifdef __i386_on_x86_64__
    static char pcap_version_str[80];
    memccpy(pcap_version_str, pcap_lib_version(), 0, sizeof(pcap_version_str) - 1);
    TRACE("%s\n", debugstr_a(pcap_version_str));
    return pcap_version_str;
#else
    const char* ret = pcap_lib_version();
    TRACE("%s\n", debugstr_a(ret));
    return ret;
#endif
}

int CDECL wine_pcap_list_datalinks(wine_pcap_t *p, int **dlt_buffer)
{
    int * HOSTPTR buf;
    int ret;
    TRACE("(%p %p)\n", p, dlt_buffer);
    ret = pcap_list_datalinks(pcap_get(p), &buf);
    return pcap_conv_datalinks(buf, ret, dlt_buffer);
}

char* CDECL wine_pcap_lookupdev(char *errbuf)
{
    static char *ret;
    pcap_if_t *devs;

    TRACE("(%p)\n", errbuf);
    if (!ret)
    {
        if (pcap_findalldevs( &devs, errbuf ) == -1) return NULL;
        if (!devs) return NULL;
        if ((ret = heap_alloc( strlen(devs->name) + 1 ))) strcpy( ret, devs->name );
        pcap_freealldevs( devs );
    }
    return ret;
}

int CDECL wine_pcap_lookupnet(const char *device, unsigned int *netp, unsigned int *maskp,
                              char *errbuf)
{
    TRACE("(%s %p %p %p)\n", debugstr_a(device), netp, maskp, errbuf);
    return pcap_lookupnet(device, netp, maskp, errbuf);
}

int CDECL wine_pcap_loop(wine_pcap_t *p, int cnt,
                         void (CALLBACK *callback)(u_char *, const struct pcap_pkthdr * WIN32PTR, const u_char *),
                         unsigned char *user)
{
    TRACE("(%p %i %p %p)\n", p, cnt, callback, user);

    if (callback)
    {
        PCAP_HANDLER_CALLBACK pcb;
        pcb.pfn_cb = callback;
        pcb.user_data = user;
        pcb.wp = p;
        return pcap_loop(pcap_get(p), cnt, pcap_handler_callback, (unsigned char *)&pcb);
    }

    return pcap_loop(pcap_get(p), cnt, NULL, user);
}

int CDECL wine_pcap_major_version(wine_pcap_t *p)
{
    TRACE("(%p)\n", p);
    return pcap_major_version(pcap_get(p));
}

int CDECL wine_pcap_minor_version(wine_pcap_t *p)
{
    TRACE("(%p)\n", p);
    return pcap_minor_version(pcap_get(p));
}

int CDECL wine_pcap_next_ex(wine_pcap_t *p, struct pcap_pkthdr * WIN32PTR *pkt_header, const unsigned char **pkt_data)
{
    const u_char * HOSTPTR data = NULL;
    struct pcap_pkthdr *h = NULL;
    int ret;
    TRACE("(%p %p %p)\n", p, pkt_header, pkt_data);
    if (!(ret = pcap_next_ex(pcap_get(p), &h, &data)))
    {
        pcap_dup_pkt_header(p, h, pkt_header);
        ret = pcap_dup_pkt(p, h, data, pkt_data);
    }
    return ret;
}

const unsigned char* CDECL wine_pcap_next(wine_pcap_t *p, struct pcap_pkthdr * WIN32PTR h)
{
    const unsigned char *pkt_data;
    TRACE("(%p %p)\n", p, h);
    if (!wine_pcap_next_ex(p, &h, &pkt_data))
        return pkt_data;
    return NULL;
}

#ifndef PCAP_OPENFLAG_PROMISCUOUS
#define PCAP_OPENFLAG_PROMISCUOUS 1
#endif

wine_pcap_t* CDECL wine_pcap_open(const char *source, int snaplen, int flags, int read_timeout,
                             void *auth, char *errbuf)
{
    int promisc = flags & PCAP_OPENFLAG_PROMISCUOUS;
    FIXME("(%s %i %i %i %p %p): partial stub\n", debugstr_a(source), snaplen, flags, read_timeout,
                                                 auth, errbuf);
    return pcap_wrap(pcap_open_live(source, snaplen, promisc, read_timeout, errbuf));
}

wine_pcap_t* CDECL wine_pcap_open_live(const char *source, int snaplen, int promisc, int to_ms,
                                  char *errbuf)
{
    TRACE("(%s %i %i %i %p)\n", debugstr_a(source), snaplen, promisc, to_ms, errbuf);
    return pcap_wrap(pcap_open_live(source, snaplen, promisc, to_ms, errbuf));
}

int CDECL wine_pcap_parsesrcstr(const char *source, int *type, char *host, char *port, char *name, char *errbuf)
{
    int t = PCAP_SRC_IFLOCAL;
    const char *p = source;

    FIXME("(%s %p %p %p %p %p): partial stub\n", debugstr_a(source), type, host, port, name, errbuf);

    if (host)
        *host = '\0';
    if (port)
        *port = '\0';
    if (name)
        *name = '\0';

    if (!strncmp(p, PCAP_SRC_IF_STRING, strlen(PCAP_SRC_IF_STRING)))
        p += strlen(PCAP_SRC_IF_STRING);
    else if (!strncmp(p, PCAP_SRC_FILE_STRING, strlen(PCAP_SRC_FILE_STRING)))
    {
        p += strlen(PCAP_SRC_FILE_STRING);
        t = PCAP_SRC_FILE;
    }

    if (type)
        *type = t;

    if (!*p)
    {
        if (errbuf)
            sprintf(errbuf, "The name has not been specified in the source string.");
        return -1;
    }

    if (name)
        strcpy(name, p);

    return 0;
}

int CDECL wine_pcap_sendpacket(wine_pcap_t *p, const unsigned char *buf, int size)
{
    TRACE("(%p %p %i)\n", p, buf, size);
    return pcap_sendpacket(pcap_get(p), buf, size);
}

int CDECL wine_pcap_set_datalink(wine_pcap_t *p, int dlt)
{
    TRACE("(%p %i)\n", p, dlt);
    return pcap_set_datalink(pcap_get(p), dlt);
}

int CDECL wine_pcap_setbuff(wine_pcap_t *p, int dim)
{
    FIXME("(%p %i) stub\n", p, dim);
    return 0;
}

int CDECL wine_pcap_setfilter(wine_pcap_t *p, wine_bpf_program *fp)
{
    struct bpf_program prog = { fp->bf_len, fp->bf_insns };
    TRACE("(%p %p)\n", p, fp);
    return pcap_setfilter(pcap_get(p), &prog);
}

int CDECL wine_pcap_setnonblock(wine_pcap_t *p, int nonblock, char *errbuf)
{
    TRACE("(%p %i %p)\n", p, nonblock, errbuf);
    return pcap_setnonblock(pcap_get(p), nonblock, errbuf);
}

int CDECL wine_pcap_snapshot(wine_pcap_t *p)
{
    TRACE("(%p)\n", p);
    return pcap_snapshot(pcap_get(p));
}

int CDECL wine_pcap_stats(wine_pcap_t *p, struct pcap_stat *ps)
{
    TRACE("(%p %p)\n", p, ps);
    return pcap_stats(pcap_get(p), ps);
}

int CDECL wine_wsockinit(void)
{
    WSADATA wsadata;
    TRACE("()\n");
    if (WSAStartup(MAKEWORD(1,1), &wsadata)) return -1;
    return 0;
}

pcap_dumper_t* CDECL wine_pcap_dump_open(wine_pcap_t *p, const char *fname)
{
    pcap_dumper_t *dumper;
    WCHAR *fnameW = heap_strdupAtoW(fname);
    char *unix_path;

    TRACE("(%p %s)\n", p, debugstr_a(fname));

    unix_path = wine_get_unix_file_name(fnameW);
    heap_free(fnameW);
    if(!unix_path)
        return NULL;

    TRACE("unix_path %s\n", debugstr_a(unix_path));

    dumper = pcap_dump_open(pcap_get(p), unix_path);
    heap_free(unix_path);

    return dumper;
}

void CDECL wine_pcap_dump(u_char *user, const struct pcap_pkthdr * WIN32PTR h, const u_char *sp)
{
    TRACE("(%p %p %p)\n", user, h, sp);
    return pcap_dump(user, h, sp);
}

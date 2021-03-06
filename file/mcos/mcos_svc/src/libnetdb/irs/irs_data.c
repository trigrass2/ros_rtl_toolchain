/*  $NetBSD: irs_data.c,v 1.1.1.3.4.1 2007/05/17 00:39:55 jdc Exp $ */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "port_before.h"

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include "resolv.h"
#include <stdio.h>
#include <string.h>
#include "isc/memcluster.h"

#ifdef DO_PTHREADS
#include <pthread.h>
#endif

#include "irs.h"
#include <stdlib.h>

#include "port_after.h"

#include "irs_data.h"
#undef _res

struct __res_state _res;

#ifdef  DO_PTHREADS
static pthread_key_t    key;
static int              once = 0;
#else
static struct net_data  *net_data;
#endif

void
irs_destroy(void) {
#ifndef DO_PTHREADS
    if (net_data != NULL)
        net_data_destroy(net_data);
    net_data = NULL;
#endif
}

void
net_data_destroy(void *p) {
    struct net_data *net_data = p;

    res_ndestroy(net_data->res);
#ifdef WANT_IRS_GR
    if (net_data->gr != NULL) {
        (*net_data->gr->close)(net_data->gr);
        net_data->gr = NULL;
    }
#endif
#ifdef WANT_IRS_PW
    if (net_data->pw != NULL) {
        (*net_data->pw->close)(net_data->pw);
        net_data->pw = NULL;
    }
#endif
    if (net_data->sv != NULL) {
        (*net_data->sv->close)(net_data->sv);
        net_data->sv = NULL;
    }
#ifdef WANT_IRS_PR
    if (net_data->pr != NULL) {
        (*net_data->pr->close)(net_data->pr);
        net_data->pr = NULL;
    }
#endif
    if (net_data->ho != NULL) {
        (*net_data->ho->close)(net_data->ho);
        net_data->ho = NULL;
    }
#ifdef WANT_IRS_NW
    if (net_data->nw != NULL) {
        (*net_data->nw->close)(net_data->nw);
        net_data->nw = NULL;
    }
#endif
#ifdef WANT_IRS_NG
    if (net_data->ng != NULL) {
        (*net_data->ng->close)(net_data->ng);
        net_data->ng = NULL;
    }
#endif
    if (net_data->ho_data != NULL) {
        free(net_data->ho_data);
        net_data->ho_data = NULL;
    }
    if (net_data->nw_data != NULL) {
        free(net_data->nw_data);
        net_data->nw_data = NULL;
    }

    (*net_data->irs->close)(net_data->irs);
    memput(net_data, sizeof *net_data);
}

/*
 *  applications that need a specific config file other than
 * _PATH_IRS_CONF should call net_data_init directly rather than letting
 *   the various wrapper functions make the first call. - brister
 */

struct net_data *
net_data_init(const char *conf_file) {
#ifdef  DO_PTHREADS
#ifndef LIBBIND_MUTEX_INITIALIZER
#define LIBBIND_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif
    static pthread_mutex_t keylock = LIBBIND_MUTEX_INITIALIZER;
    struct net_data *net_data;

    if (!once) {
        if (pthread_mutex_lock(&keylock) != 0)
            return (NULL);
        if (!once) {
            if (pthread_key_create(&key, net_data_destroy) != 0)
                return (NULL);
            once = 1;
        }
        if (pthread_mutex_unlock(&keylock) != 0)
            return (NULL);
    }
    net_data = pthread_getspecific(key);
#endif

    if (net_data == NULL) {
        net_data = net_data_create(conf_file);
        if (net_data == NULL)
            return (NULL);
#ifdef  DO_PTHREADS
        if (pthread_setspecific(key, net_data) != 0) {
            net_data_destroy(net_data);
            return (NULL);
        }
#endif
    }

    return (net_data);
}

static void net_data_create_init_stayopen(struct net_data *net_data)
{
    /* Set all stayopen flag */
    net_data->gr_stayopen = 1;
    net_data->pw_stayopen = 1;
    net_data->sv_stayopen = 1;
    net_data->pr_stayopen = 1;
    net_data->ho_stayopen = 1;
    net_data->nw_stayopen = 1;
}

struct net_data *
net_data_create(const char *conf_file) {
    struct net_data *net_data;

    net_data = memget(sizeof (struct net_data));
    if (net_data == NULL)
        return (NULL);
    memset(net_data, 0, sizeof (struct net_data));
    net_data_create_init_stayopen(net_data);

    if ((net_data->irs = irs_gen_acc("", conf_file)) == NULL) {
        memput(net_data, sizeof (struct net_data));
        return (NULL);
    }
#ifndef DO_PTHREADS
    (*net_data->irs->res_set)(net_data->irs, &_res, NULL);
#endif

    net_data->res = (*net_data->irs->res_get)(net_data->irs);
    if (net_data->res == NULL) {
        (*net_data->irs->close)(net_data->irs);
        memput(net_data, sizeof (struct net_data));
        return (NULL);
    }

    if ((net_data->res->options & RES_INIT) == 0U &&
        res_ninit(net_data->res) == -1) {
        (*net_data->irs->close)(net_data->irs);
        memput(net_data, sizeof (struct net_data));
        return (NULL);
    }

    return (net_data);
}

void
net_data_minimize(struct net_data *net_data) {
        res_nclose(net_data->res);
}

#ifdef _REENTRANT
struct __res_state *
__res_state(void) {
    /* NULL param here means use the default config file. */
    struct net_data *net_data = net_data_init(NULL);
    if (net_data && net_data->res)
        return (net_data->res);

    return (&_res);
}
#endif

int *
__h_errno(void) {
    /* NULL param here means use the default config file. */
    struct net_data *net_data = net_data_init(NULL);
    if (net_data && net_data->res)
        return (&net_data->res->res_h_errno);

    return (&h_errno);
}

void
__h_errno_set(struct __res_state *res, int err) {

    res->res_h_errno = err;
}

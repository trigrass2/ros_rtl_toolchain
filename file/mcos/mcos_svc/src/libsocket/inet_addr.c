/*  $NetBSD: inet_addr.c,v 1.1.1.1.2.1.2.1 2005/02/06 07:43:51 jmc Exp $    */

/*
 * ++Copyright++ 1983, 1990, 1993
 * -
 * Copyright (c) 1983, 1990, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include <sys/param.h>

#ifdef MCOS
/* Missing prototype in compiler ctype.h */
#ifndef isascii
int isascii(int);
#endif
#endif
int inet_aton (const char *, struct in_addr *);

/*
 * Because the ctype(3) posix definition, if used "safely" in code everywhere,
 * would mean all normal code that walks through strings needed casts.  Yuck.
 */
#define ISALNUM(x)  isalnum((u_char)(x))
#define ISALPHA(x)  isalpha((u_char)(x))
#define ISASCII(x)  isascii((u_char)(x))
#define ISDIGIT(x)  isdigit((u_char)(x))
#define ISPRINT(x)  isprint((u_char)(x))
#define ISSPACE(x)  isspace((u_char)(x))
#define ISUPPER(x)  isupper((u_char)(x))
#define ISXDIGIT(x) isxdigit((u_char)(x))
#define ISLOWER(x)  islower((u_char)(x))

/* a part of inet_aton */
#if defined __aarch64__
static int inet_aton_set_addr(struct in_addr *addr, unsigned int parts[], int n, unsigned int val)
#else
static int inet_aton_set_addr(struct in_addr *addr, unsigned int parts[], int n, unsigned long val)
#endif
{
    switch (n) {

    case 0:
        return (0);     /* initial nondigit */

    case 1:             /* a -- 32 bits */
        break;

    case 2:             /* a.b -- 8.24 bits */
        if (val > 0xffffff)
            return (0);
        val |= parts[0] << 24;
        break;

    case 3:             /* a.b.c -- 8.8.16 bits */
        if (val > 0xffff)
            return (0);
        val |= (parts[0] << 24) | (parts[1] << 16);
        break;

    case 4:             /* a.b.c.d -- 8.8.8.8 bits */
        if (val > 0xff)
            return (0);
        val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
        break;
    }
    if (addr)
    {
        addr->s_addr = htonl(val);
    }
    return (1);
}

/*
 * Check whether "cp" is a valid ascii representation
 * of an Internet address and convert to a binary address.
 * Returns 1 if the address is valid, 0 if not.
 * This replaces inet_addr, the return value from which
 * cannot distinguish between failure and a local broadcast address.
 */
int inet_aton(const char *cp, struct in_addr *addr)
{
#if defined __aarch64__
    unsigned long val;
#else
    unsigned int val;
#endif
    int base, n;
    char c;
    unsigned int parts[4];
    unsigned int *pp = parts;

    c = *cp;
    for (;;)
    {
        /*
         * Collect number up to ``.''.
         * Values are specified as for C:
         * 0x=hex, 0=octal, isdigit=decimal.
         */
        if (!ISDIGIT(c))
        {
            return (0);
        }
        val = 0; base = 10;
        if (c == '0')
        {
            c = *++cp;
            if (c == 'x' || c == 'X')
            {
                base = 16, c = *++cp;
            }
            else
            {
                base = 8;
            }
        }
        for (;;)
        {
            if (ISASCII(c) && ISDIGIT(c))
            {
                val = (val * base) + (c - '0');
                c = *++cp;
            }
            else if (base == 16 && ISASCII(c) && ISXDIGIT(c))
            {
                val = (val << 4) |
                    (c + 10 - (ISLOWER(c) ? 'a' : 'A'));
                c = *++cp;
            }
            else
            {
                break;
            }
        }
        if (c == '.')
        {
            /*
             * Internet format:
             *  a.b.c.d
             *  a.b.c   (with c treated as 16 bits)
             *  a.b (with b treated as 24 bits)
             */
            if (pp >= parts + 3)
            {
                return (0);
            }
            *pp++ = val;
            c = *++cp;
        }
        else
        {
            break;
        }
    }
    /*
     * Check for trailing characters.
     */
    if (c != '\0' && (!ISASCII(c) || !ISSPACE(c)))
        return (0);
    /*
     * Concoct the address according to
     * the number of parts specified.
     */
    n = pp - parts + 1;
    return inet_aton_set_addr(addr, parts, n, val);
}

/*
 * Ascii internet address interpretation routine.
 * The value returned is in network order.
 */

in_addr_t inet_addr(const char *cp)
{
    in_addr_t    sta;
    struct in_addr val;

    sta = (0xffffffff);
    if (cp != 0)
    {
        if (inet_aton(cp, &val))
        {
            sta = val.s_addr;
        }
    }
    return sta;
}
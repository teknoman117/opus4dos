/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdlib.h>
#include <string.h>

#include "ogg.h"

static unsigned long
le32(const unsigned char *p)
{
    return p[0] | ((unsigned long) p[1] << 8) | ((unsigned long) p[2] << 16) | ((unsigned long) p[3] << 24);
}

static long long
le64(const unsigned char *p)
{
    return (long long) ((unsigned long long) le32(p) | ((unsigned long long) le32(p + 4) << 32));
}

int
ogg_open(ogg_reader *r, const char *path)
{
    memset(r, 0, sizeof(*r));
    r->f = fopen(path, "rb");
    return r->f ? 0 : -1;
}

void
ogg_close(ogg_reader *r)
{
    if (r->f)
        fclose(r->f);
    free(r->body);
    free(r->pkt);
    memset(r, 0, sizeof(*r));
}

/* The next page of our stream; 0, or -1. */
static int
next_page(ogg_reader *r)
{
    for (;;) {
        unsigned long len = 0, serial;
        unsigned      i;

        if ((fread(r->hdr, 1, 27, r->f) != 27) || memcmp(r->hdr, "OggS", 4) || r->hdr[4])
            return -1;
        r->nseg = r->hdr[26];
        if (fread(r->hdr + 27, 1, r->nseg, r->f) != r->nseg)
            return -1;
        for (i = 0; i < r->nseg; i++)
            len += r->hdr[27 + i];
        if (len > r->body_cap) {
            unsigned char *b = (unsigned char *) realloc(r->body, len);
            if (!b)
                return -1;
            r->body     = b;
            r->body_cap = len;
        }
        if (fread(r->body, 1, len, r->f) != len)
            return -1;
        serial = le32(r->hdr + 14);
        if (!r->have_serial) {
            r->serial      = serial;
            r->have_serial = 1;
        }
        if (serial != r->serial)
            continue; /* another logical stream's page */
        r->seg          = 0;
        r->body_pos     = 0;
        r->page_eos     = (r->hdr[5] & 0x04) != 0;
        r->page_granule = le64(r->hdr + 6);
        return 0;
    }
}

int
ogg_packet(ogg_reader *r)
{
    r->pkt_len = 0;
    for (;;) {
        unsigned lace;

        if (r->seg == r->nseg) {
            if (next_page(r))
                return -1;
            /* A packet continued from the last page goes on; one that isn't, starts afresh. */
            if (!(r->hdr[5] & 0x01))
                r->pkt_len = 0;
            continue;
        }
        lace = r->hdr[27 + r->seg++];
        if (r->pkt_len + lace > r->pkt_cap) {
            unsigned long  cap = r->pkt_len + lace + 4096;
            unsigned char *p   = (unsigned char *) realloc(r->pkt, cap);
            if (!p)
                return -1;
            r->pkt     = p;
            r->pkt_cap = cap;
        }
        memcpy(r->pkt + r->pkt_len, r->body + r->body_pos, lace);
        r->pkt_len += lace;
        r->body_pos += lace;
        if (lace < 255) {
            /* Is it the last packet the page finishes? */
            unsigned i;
            int      last = 1;
            for (i = r->seg; i < r->nseg; i++) {
                if (r->hdr[27 + i] < 255) {
                    last = 0;
                    break;
                }
            }
            r->granule = last ? r->page_granule : -1;
            r->eos     = last && r->page_eos;
            return 0;
        }
    }
}

long long
ogg_last_granule(const char *path, unsigned long serial)
{
    FILE          *f = fopen(path, "rb");
    long           size, from;
    unsigned char *tail;
    long           n, i;
    long long      g = -1;

    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    from = (size > 65536L) ? size - 65536L : 0;
    tail = (unsigned char *) malloc(size - from);
    if (tail) {
        fseek(f, from, SEEK_SET);
        n = (long) fread(tail, 1, size - from, f);
        for (i = 0; i + 27 <= n; i++) {
            if (!memcmp(tail + i, "OggS", 4) && !tail[i + 4] && (le32(tail + i + 14) == serial)) {
                long long pg = le64(tail + i + 6);
                if (pg != -1)
                    g = pg;
            }
        }
        free(tail);
    }
    fclose(f);
    return g;
}

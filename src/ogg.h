/* SPDX-License-Identifier: BSD-3-Clause */
/* Reading an Ogg file's packets a page at a time (RFC 3533), for one logical stream: the first
   one in the file. */
#ifndef OPUS4DOS_OGG_H
#define OPUS4DOS_OGG_H

#include <stdio.h>

typedef struct {
    FILE          *f;
    unsigned char  hdr[27 + 255];
    unsigned char *body;
    unsigned long  body_cap;
    unsigned       nseg, seg;
    unsigned long  body_pos;
    unsigned long  serial;
    int            have_serial;
    int            page_eos;
    long long      page_granule;
    /* The packet ogg_packet() gives. */
    unsigned char *pkt;
    unsigned long  pkt_len, pkt_cap;
    long long      granule; /* the page's granule position, if the packet ends the page, else -1 */
    int            eos;     /* the packet ends the stream's last page */
} ogg_reader;

int  ogg_open(ogg_reader *r, const char *path);
/* The next packet in r->pkt; 0, or -1 at the end (or on a damaged file). */
int  ogg_packet(ogg_reader *r);
void ogg_close(ogg_reader *r);

/* The granule position of the last page in the file of stream `serial`, -1 if none is found:
   for Opus, where the stream ends, in 48 kHz samples (plus the pre-skip). */
long long ogg_last_granule(const char *path, unsigned long serial);

#endif

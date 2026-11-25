/* SPDX-License-Identifier: BSD-3-Clause */
/* Linear-interpolation resampling of 16-bit PCM, for a card that can't play any of the rates the
   Opus decoder gives (48, 24, 16, 12 or 8 kHz), as the Sound Blaster 16 can't go above 44.1. */
#ifndef OPUS4DOS_RESAMPLE_H
#define OPUS4DOS_RESAMPLE_H

typedef struct {
    unsigned long long step;    /* input samples per output sample, 32.32 */
    unsigned long long pos;     /* where the next output sample is, 32.32, from last[] */
    short              last[2]; /* the last input frame of the previous call */
    int                channels;
} resampler;

void resample_init(resampler *r, long in_rate, long out_rate, int channels);

/* Resamples n frames of in into out, which has room for n * out_rate / in_rate + 2 frames;
   returns the frames written. */
long resample(resampler *r, const short *in, long n, short *out);

#endif

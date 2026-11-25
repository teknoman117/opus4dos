/* SPDX-License-Identifier: BSD-3-Clause */
#include "resample.h"

void
resample_init(resampler *r, long in_rate, long out_rate, int channels)
{
    /* 32 fraction bits: a 16-bit step (71331/65536 for 48 to 44.1 kHz) is short by 4 parts in a
       million, which drifts the output about 11 samples behind in a minute. */
    r->step     = ((unsigned long long) in_rate << 32) / (unsigned long) out_rate;
    r->pos      = 1ull << 32; /* the first output sample is the first input sample */
    r->last[0]  = r->last[1] = 0;
    r->channels = channels;
}

long
resample(resampler *r, const short *in, long n, short *out)
{
    long produced = 0;
    int  ch       = r->channels, c;

    if (n <= 0)
        return 0;
    /* Input frame k is at position k + 1; position 0 is last[], the frame before. An output
       sample at position p lies between frames p >> 32 and the one after. */
    while ((unsigned long) (r->pos >> 32) < (unsigned long) n) {
        unsigned long i = (unsigned long) (r->pos >> 32);
        long          f = (long) ((unsigned long) (r->pos >> 17) & 0x7fff); /* 15 bits */

        for (c = 0; c < ch; c++) {
            long a = (i == 0) ? r->last[c] : in[(i - 1) * ch + c];
            long b = in[i * ch + c];
            *out++ = (short) (a + (((b - a) * f) >> 15));
        }
        produced++;
        r->pos += r->step;
    }
    for (c = 0; c < ch; c++)
        r->last[c] = in[(n - 1) * ch + c];
    r->pos -= (unsigned long long) n << 32;
    return produced;
}

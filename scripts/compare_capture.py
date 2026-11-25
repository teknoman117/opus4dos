#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Compare an emulator's recorded sound output with what OPUS4DOS sent to the card.

Usage: compare_capture.py <recording.wav> <sent.wav> [--plot out.png]

Both are 16-bit PCM WAVs. The recording is resampled to the sent file's rate if they differ,
both are summed to mono, and the recording is searched for the sent audio by cross-correlation.
Prints where it starts, the gain the emulator's mixer applied, the correlation, and the SNR of
the recording against the sent audio at that gain: a match means the card played the music.
"""
import sys
import wave

import numpy as np


def load(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            raise SystemExit(f"{path}: not 16-bit")
        rate, ch = w.getframerate(), w.getnchannels()
        x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
    return rate, x.reshape(-1, ch).mean(axis=1)


def resample(x, rate_from, rate_to):
    if rate_from == rate_to:
        return x
    n = int(round(len(x) * rate_to / rate_from))
    return np.interp(np.arange(n) * rate_from / rate_to, np.arange(len(x)), x)


args = sys.argv[1:]
plot = None
if "--plot" in args:
    i = args.index("--plot")
    plot = args[i + 1]
    del args[i:i + 2]
rec_rate, rec = load(args[0])
ref_rate, ref = load(args[1])
rec = resample(rec, rec_rate, ref_rate)

# Where the sent audio starts in the recording: cross-correlate a 10 s window of it.
win = ref[ref_rate * 5: ref_rate * 15]
nfft = 1 << int(np.ceil(np.log2(len(rec) + len(win))))
xc = np.fft.irfft(np.fft.rfft(rec, nfft) * np.conj(np.fft.rfft(win, nfft)), nfft)[: len(rec)]
lag = int(np.argmax(np.abs(xc))) - ref_rate * 5

a = rec[max(lag, 0):]
b = ref[max(-lag, 0):]
n = min(len(a), len(b))
a, b = a[:n], b[:n]
# Plain sums rather than np.dot/np.corrcoef: this machine's BLAS dies of an illegal instruction.
gain = float(np.sum(a * b) / np.sum(b * b))
err = a - gain * b
am, bm = a - a.mean(), b - b.mean()
corr = float(np.sum(am * bm) / np.sqrt(np.sum(am * am) * np.sum(bm * bm)))
snr = 10 * np.log10(np.sum((gain * b) ** 2) / max(np.sum(err**2), 1e-9))
print(f"{args[0]}: the sent audio starts at {lag / ref_rate:.3f} s; mixer gain {gain:.3f} "
      f"({20 * np.log10(abs(gain)):+.1f} dB); correlation {corr:.5f}; SNR {snr:.1f} dB over {n / ref_rate:.1f} s")

if plot:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    t = np.arange(n) / ref_rate
    fig, ax = plt.subplots(2, 1, figsize=(11, 5), sharex=True)
    ax[0].plot(t, gain * b, lw=0.3, color="#4a7bd0")
    ax[0].set_title("what OPUS4DOS sent to the card (scaled by the mixer gain)", fontsize=9)
    ax[1].plot(t, a, lw=0.3, color="#d0704a")
    ax[1].set_title("what the emulator played", fontsize=9)
    ax[1].set_xlabel("seconds")
    fig.tight_layout()
    fig.savefig(plot, dpi=110)

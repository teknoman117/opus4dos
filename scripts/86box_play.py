#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Play Opus files with OPUS4DOS in 86Box, headless, and record the sound card's output.

Usage: 86box_play.py <86Box> <OPUS4DOS.EXE> <out dir> <file.opu>... [--timeout <host s>] [--watch]

The machine is an ASUS P/I-P55T2P4 (430HX) with a Pentium MMX 200, 32 MB, a Cirrus GD5446 and a
Sound Blaster 16 at 220h, IRQ 5, DMA 1/5. Drive A: is the FreeDOS 1.4 boot floppy, which sets
BLASTER and runs OPUS4DOS on each file; drive C: is a 64 MB IDE disk (FAT16, made with sfdisk
and mtools) holding OPUS4DOS and the files, where OPUS4DOS's console output goes too
(C:\\LOG.TXT). The BIOS finds the disk itself, so the machine needs no CMOS set up. 86Box's automation records the
sound output in emulated time (so it doesn't matter how fast the host runs it) into <out dir>,
with the log as <out dir>/LOG.TXT. 86Box needs AUTOMATION=ON.
"""
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

BOOT = os.path.expanduser("~/Downloads/FD14-FloppyEdition/144m/x86BOOT.img")

CONFIG = """[Machine]
machine = p55t2p4
cpu_family = pentium_p55c
cpu_speed = 200000000
cpu_multi = 3
cpu_use_dynarec = 1
mem_size = 32768

[Video]
gfxcard = cl_gd5446_pci

[Sound]
sndcard = sb16

[Floppy and CD-ROM drives]
fdd_01_type = 35_2hd
fdd_01_fn = {boot}
fdd_02_type = none

[Hard disks]
hdd_01_parameters = 63, 16, 130, 0, ide
hdd_01_fn = {data}
hdd_01_ide_channel = 0:0
"""
CYLS, HEADS, SECTORS = 130, 16, 63

args = sys.argv[1:]
timeout_s, watch = 1800, False
if "--timeout" in args:
    i = args.index("--timeout")
    timeout_s = int(args[i + 1])
    del args[i:i + 2]
if "--watch" in args:
    args.remove("--watch")
    watch = True
emu, exe, out = args[0], args[1], Path(args[2]).resolve()
files = args[3:]
out.mkdir(parents=True, exist_ok=True)


def mtools(*a):
    subprocess.run(list(a), check=True)


with tempfile.TemporaryDirectory(prefix="86box_play_") as tmp:
    root = Path(tmp)
    boot, data = root / "boot.img", root / "hdd.img"
    shutil.copy(BOOT, boot)
    mtools("mdel", "-i", str(boot), "::SETUP.BAT")
    (root / "FDCONFIG.SYS").write_text("FILES=40\r\nBUFFERS=20\r\nLASTDRIVE=Z\r\n"
                                       "SHELL=\\FREEDOS\\BIN\\COMMAND.COM \\FREEDOS\\BIN /E:2048 /P=\\FDAUTO.BAT\r\n")
    lines = ["@ECHO OFF", "SET PATH=\\FREEDOS\\BIN", "SET BLASTER=A220 I5 D1 H5 T6", "C:", "ECHO %BLASTER% > LOG.TXT"]
    for f in files:
        lines.append(f"OPUS4DOS {os.path.basename(f).upper()} >> LOG.TXT")
    lines.append("ECHO OPUSDONE")
    (root / "FDAUTO.BAT").write_text("\r\n".join(lines) + "\r\n")
    for name in ("FDCONFIG.SYS", "FDAUTO.BAT"):
        mtools("mcopy", "-o", "-i", str(boot), str(root / name), "::" + name)
    # The disk: one FAT16 partition from sector 63 (the first head of cylinder 0 skipped).
    with open(data, "wb") as d:
        d.truncate(CYLS * HEADS * SECTORS * 512)
    subprocess.run(["sfdisk", "--no-reread", "-q", str(data)], input=b"start=63, type=6, bootable\n",
                   check=True, stdout=subprocess.DEVNULL)
    # This BIOS boots C: before A:, so the disk's MBR boots the floppy (scripts/vm/floppymbr.asm).
    mbr = (Path(__file__).resolve().parent / "vm" / "floppymbr.bin").read_bytes()
    with open(data, "r+b") as d:
        d.write(mbr[:0x1be])
    part = f"{data}@@{63 * 512}"
    mtools("mformat", "-i", part, "-T", str(CYLS * HEADS * SECTORS - 63), "-h", str(HEADS), "-s", str(SECTORS),
           "-H", "63", "-v", "OPUS4DOS", "::")
    mtools("mcopy", "-i", part, exe, "::OPUS4DOS.EXE")
    for f in files:
        mtools("mcopy", "-i", part, f, "::" + os.path.basename(f).upper())

    (root / "86box.cfg").write_text(CONFIG.format(boot=boot, data=data))
    aud = root / "audio"
    aud.mkdir()
    sock = root / "a.sock"
    env = dict(os.environ)
    env.setdefault("QT_QPA_PLATFORM", "offscreen")
    p = subprocess.Popen([emu, "-N", "-P", str(root), "-O", str(root / "g.cfg"), "-L", str(root / "log"),
                          "--automation", str(sock), "--automation-hold"],
                         env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(300):
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.connect(str(sock))
                break
            except OSError:
                s.close()
                time.sleep(0.1)
        else:
            raise SystemExit("86Box didn't open its automation socket")
        fh = s.makefile("rwb", buffering=0)

        def req(line):
            fh.write((line + "\n").encode())
            r = json.loads(fh.readline())
            if not r.get("ok"):
                raise RuntimeError(f"{line!r}: {r}")
            return r

        def screen():
            try:
                t = req("text")
            except RuntimeError:
                return ""
            return "\n".join(bytes.fromhex(row).decode("cp437", "replace").rstrip() for row in t.get("chars", []))

        req(f"audio start {aud}")
        t0 = time.time()
        done = False
        while time.time() - t0 < timeout_s:
            r = req("wait text 10000 OPUSDONE")
            if r.get("matched"):
                done = True
                break
            scr = screen()
            if watch:
                tail = [l for l in scr.splitlines() if l.strip()][-2:]
                print(f"[{time.time() - t0:5.0f} s host, {r.get('ms', 0) / 1000:6.1f} s emulated] " + " | ".join(tail),
                      flush=True)
            if "Press F1" in scr:
                req("key down 0x3b")
                req("key up 0x3b")
        rec = req("audio stop")
        print(f"[{'finished' if done else 'TIMED OUT'} after {time.time() - t0:.0f} s of host time]")
        if not done:
            print(screen())
        try:
            req("screenshot " + str(out / "screen.png"))
        except RuntimeError:
            pass
        for st in rec.get("streams", []):
            if st.get("file"):
                dst = out / f"86box_{st['name']}_{st['segment']}.wav"
                shutil.copy(aud / st["file"], dst)
                print(f"  {st['name']} segment {st['segment']}: {st['rate']} Hz, "
                      f"{st['frames'] / st['rate']:.1f} s -> {dst}")
        fh.write(b"quit\n")
        p.wait(timeout=30)
    finally:
        if p.poll() is None:
            p.kill()
    r = subprocess.run(["mtype", "-i", part, "::LOG.TXT"], capture_output=True)
    (out / "LOG.TXT").write_bytes(r.stdout)

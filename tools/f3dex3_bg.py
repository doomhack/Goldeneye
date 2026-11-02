#!/usr/bin/env python3
"""
Converts a level's room display lists from Fast3D to F3DEX3 (make UCODE=f3dex3).

The room display lists are the pri_mapping_binary_N and sec_mapping_binary_N arrays in
assets/obseg/bg/bg_*_all_p.c, each compressed in Rare's 1172 format (the bytes 0x11 0x72
then a raw deflate stream). This script writes a copy of the .c file with every such
array decompressed, translated command by command, and compressed again. The rest of
the file is copied unchanged.

Room display lists use only a few commands: vertex loads, triangles, geometry and other
modes, texture settings, end of list, GE's texture command G_SETTEX (0xC0, expanded when
the room loads) and some RDP commands (which are the same in both microcodes).

    python3 tools/f3dex3_bg.py in.c out.c
"""

import re
import struct
import sys
import zlib

# Fast3D opcodes
F3D_SPNOOP = 0x00
F3D_VTX = 0x04
F3D_TRI4 = 0xB1  # GE's four-triangle command
F3D_CLEARGEOMETRYMODE = 0xB6
F3D_SETGEOMETRYMODE = 0xB7
F3D_ENDDL = 0xB8
F3D_SETOTHERMODE_L = 0xB9
F3D_SETOTHERMODE_H = 0xBA
F3D_TEXTURE = 0xBB
F3D_TRI1 = 0xBF

# F3DEX3 opcodes
EX3_NOOP = 0x00
EX3_VTX = 0x01
EX3_TRI1 = 0x05
EX3_TRI2 = 0x06
EX3_TEXTURE = 0xD7
EX3_GEOMETRYMODE = 0xD9
EX3_ENDDL = 0xDF
EX3_SETOTHERMODE_L = 0xE2
EX3_SETOTHERMODE_H = 0xE3

# Commands that are the same in both: GE's G_SETTEX, and RDP commands
PASSTHROUGH = {0xC0} | set(range(0xE4, 0x100))

# Fast3D geometry mode bit -> F3DEX3 geometry mode bit
GEOMETRY_MODES = {
    0x00000001: 0x00000001,  # G_ZBUFFER
    0x00000004: 0x00000004,  # G_SHADE
    0x00000200: 0x00200000,  # G_SHADING_SMOOTH
    0x00001000: 0x00000200,  # G_CULL_FRONT
    0x00002000: 0x00000400,  # G_CULL_BACK
    0x00010000: 0x00010000,  # G_FOG
    0x00020000: 0x00020000,  # G_LIGHTING
    0x00040000: 0x00040000,  # G_TEXTURE_GEN
    0x00080000: 0x00080000,  # G_TEXTURE_GEN_LINEAR
    0x00100000: 0x00100000,  # G_LOD
}

# Bytes the room loader leaves between the end of the output and the end of its buffer
# when it decompresses in place (see bgLoadRoomModelData)
INPLACE_MARGIN = 0x20


class ConvertError(Exception):
    pass


def convert_geometry_mode(mode):
    out = 0
    for f3d, ex3 in GEOMETRY_MODES.items():
        if mode & f3d:
            out |= ex3
            mode &= ~f3d
    if mode:
        raise ConvertError(f"unknown geometry mode bits 0x{mode:08X}")
    return out


def tri_word(a, b, c):
    """F3DEX3 triangle indices (vertex number times 2), as in gsSP1Triangle"""
    for v in (a, b, c):
        if v >= 64:
            raise ConvertError(f"vertex index {v} out of range")
    return ((a * 2) << 16) | ((b * 2) << 8) | (c * 2)


def convert_tris(tris):
    """Draws a list of (a, b, c) with as many G_TRI2 as possible, then a G_TRI1"""
    out = []
    for i in range(0, len(tris) - 1, 2):
        out.append(((EX3_TRI2 << 24) | tri_word(*tris[i]), tri_word(*tris[i + 1])))
    if len(tris) % 2:
        out.append(((EX3_TRI1 << 24) | tri_word(*tris[-1]), 0))
    return out


def convert_command(w0, w1):
    op = w0 >> 24

    if op == F3D_SPNOOP:
        return [(EX3_NOOP << 24, 0)] if (w0, w1) == (0, 0) else _unknown(w0, w1)

    if op == F3D_VTX:
        n = ((w0 >> 20) & 0xF) + 1
        v0 = (w0 >> 16) & 0xF
        return [((EX3_VTX << 24) | (n << 12) | ((v0 + n) << 1), w1)]

    if op == F3D_TRI4:
        # Triangle i (0-3) has its first two vertices in w1 byte i and its third in
        # w0 nibble i. One with all three 0 is not drawn.
        tris = []
        for i in range(4):
            a = (w1 >> (8 * i)) & 0xF
            b = (w1 >> (8 * i + 4)) & 0xF
            c = (w0 >> (4 * i)) & 0xF
            if a or b or c:
                tris.append((a, b, c))
        return convert_tris(tris)

    if op == F3D_TRI1:
        a, b, c = ((w1 >> 16) & 0xFF) // 10, ((w1 >> 8) & 0xFF) // 10, (w1 & 0xFF) // 10
        if w1 >> 24:
            raise ConvertError(f"G_TRI1 with a flag: {w0:08X} {w1:08X}")
        return convert_tris([(a, b, c)])

    if op == F3D_CLEARGEOMETRYMODE:
        clear = convert_geometry_mode(w1)
        return [((EX3_GEOMETRYMODE << 24) | (~clear & 0xFFFFFF), 0)]

    if op == F3D_SETGEOMETRYMODE:
        return [((EX3_GEOMETRYMODE << 24) | 0xFFFFFF, convert_geometry_mode(w1))]

    if op == F3D_ENDDL:
        return [(EX3_ENDDL << 24, 0)]

    if op in (F3D_SETOTHERMODE_L, F3D_SETOTHERMODE_H):
        shift = (w0 >> 8) & 0xFF
        length = w0 & 0xFF
        newop = EX3_SETOTHERMODE_L if op == F3D_SETOTHERMODE_L else EX3_SETOTHERMODE_H
        return [((newop << 24) | ((32 - shift - length) << 8) | (length - 1), w1)]

    if op == F3D_TEXTURE:
        # Same layout except that "on" moves up a bit, and the bowtie byte goes
        level_tile = w0 & 0x3F00
        on = w0 & 0xFF
        return [((EX3_TEXTURE << 24) | level_tile | (on << 1), w1)]

    if op in PASSTHROUGH:
        return [(w0, w1)]

    return _unknown(w0, w1)


def _unknown(w0, w1):
    raise ConvertError(f"cannot convert command {w0:08X} {w1:08X}")


def convert_gdl(raw):
    """Converts a decompressed room display list. Returns (data, warning or None)"""
    warning = None
    whole = len(raw) - len(raw) % 8
    cmds = [struct.unpack(">II", raw[i:i + 8]) for i in range(0, whole, 8)]

    out = []
    for w0, w1 in cmds:
        out.extend(convert_command(w0, w1))

    # A few display lists in the original data are cut short (no G_ENDDL, and the
    # compressed data stops partway); end them properly
    if not any(w0 >> 24 == F3D_ENDDL for w0, _ in cmds):
        out.append((EX3_ENDDL << 24, 0))
        warning = "cut short, G_ENDDL added"

    return b"".join(struct.pack(">II", w0, w1) for w0, w1 in out), warning


def compress_1172(raw):
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    return b"\x11\x72" + c.compress(raw) + c.flush()


def check_inplace(comp, usize):
    """
    Checks the data still decompresses in place: the room loader puts the compressed data
    at the end of a buffer only INPLACE_MARGIN bytes bigger than the output (16-byte
    aligned), so the output must never catch up with the input still to be read.
    """
    csize = (len(comp) + 0xF) & ~0xF
    bufsize = usize + INPLACE_MARGIN
    instart = bufsize - csize
    d = zlib.decompressobj(-15)
    produced = 0
    for i in range(2, len(comp)):
        produced += len(d.decompress(comp[i:i + 1]))
        # After reading input byte i, the output may not pass the next unread byte
        if produced > instart + i + 1:
            return False
    return True


def convert_file(src):
    pattern = re.compile(r"(u32 (?:pri|sec)_mapping_binary_\d+\[\] = \{)(.*?)(\};)", re.S)
    warnings = []

    def replace(m):
        name = m.group(1).split()[1].split("[")[0]
        words = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+|\b0\b", m.group(2)) if x]
        if not any(words):
            return m.group(0)
        data = b"".join(struct.pack(">I", w) for w in words)
        if data[:2] != b"\x11\x72":
            raise ConvertError(f"{name}: not 1172 data")
        raw = zlib.decompressobj(-15).decompress(data[2:])
        try:
            gdl, warning = convert_gdl(raw)
        except ConvertError as e:
            raise ConvertError(f"{name}: {e}") from None
        if warning:
            warnings.append(f"{name}: {warning}")
        comp = compress_1172(gdl)
        if not check_inplace(comp, len(gdl)):
            raise ConvertError(f"{name}: does not decompress in place")
        comp += b"\0" * (-len(comp) % 4)
        out = [f"0x{w:08X}" for w in struct.unpack(f">{len(comp) // 4}I", comp)]
        lines = ",\n".join("    " + ", ".join(out[i:i + 8]) for i in range(0, len(out), 8))
        return f"{m.group(1)}\n{lines},\n{m.group(3)}"

    return pattern.sub(replace, src), warnings


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src = open(sys.argv[1]).read()
    try:
        out, warnings = convert_file(src)
    except ConvertError as e:
        sys.exit(f"{sys.argv[1]}: {e}")
    for w in warnings:
        print(f"{sys.argv[1]}: {w}", file=sys.stderr)
    with open(sys.argv[2], "w") as f:
        f.write(out)


if __name__ == "__main__":
    main()

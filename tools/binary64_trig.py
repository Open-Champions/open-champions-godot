#!/usr/bin/env python3
"""Correctly rounded binary64 cos/sin of the emitter angles (exact decimal arithmetic).

The pinned reference ref_particles._cs computes math.cos / math.sin of
a = s16(angle) * (2 pi / 65536), the host C library's choice of last bit: macOS arm64
and x86_64 disagree on 18,327 of the 65,536 arguments. generate_fx_exact_vectors.py
replaces _cs with cos_sin() below, and the native port (champions/fx_trig.hpp) computes
the same correctly rounded values, so x86_64, arm64 and Windows agree bit for bit.

    python3 -I tools/binary64_trig.py            # print the 16 block digests
"""
import functools
import hashlib
import math
import struct
import sys
from decimal import Decimal, localcontext

STEP = 2.0 * math.pi / 65536.0     # the reference's float constant
BLOCK = 4096                        # angles per digest row


def _bits(x):
    return struct.unpack('<Q', struct.pack('<d', x))[0]


def _series(x):
    """cos and sin of the exact binary value x (|x| <= 4) to about 75 digits."""
    with localcontext() as ctx:
        ctx.prec = 90
        x = Decimal(x)
        x2 = x * x
        c = s = Decimal(0)
        tc, ts = Decimal(1), x
        n = 0
        limit = Decimal(10) ** -80
        while abs(tc) > limit or abs(ts) > limit:
            c += tc
            s += ts
            tc = -tc * x2 / ((2 * n + 1) * (2 * n + 2))
            ts = -ts * x2 / ((2 * n + 2) * (2 * n + 3))
            n += 1
        # str() keeps every digit; float() of a decimal string rounds correctly.
        return float(str(+c)), float(str(+s))


@functools.lru_cache(maxsize=None)
def cos_sin_s16(angle):
    """(cos, sin) of s16(angle) * STEP, each the nearest binary64."""
    v = angle & 0xFFFF
    k = v - 0x10000 if v & 0x8000 else v
    return _series(float(k) * STEP)


def block_lines(block):
    """The state lines of one digest row: angles block*BLOCK - 32768 onwards."""
    out = []
    for k in range(-32768 + block * BLOCK, -32768 + (block + 1) * BLOCK):
        c, s = cos_sin_s16(k)
        out.append('C %d,%d,%d' % (k, _bits(c), _bits(s)))
    return out


def block_digest(block):
    h = hashlib.sha256()
    for line in block_lines(block):
        h.update(line.encode('ascii') + b'\n')
    return h.hexdigest()[:16]


if __name__ == '__main__':
    for b in range(65536 // BLOCK):
        print(b, block_digest(b))
    sys.exit(0)

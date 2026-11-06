#!/usr/bin/env python3
"""The application icon for packaging/package_app.sh, made from the game's own title screen.

    make_icon.py GAME_DIR OUT.png      a 1024 x 1024 RGBA PNG in the macOS icon shape

The picture is the planet seen from orbit with the two suns of Alpha Centauri, both taken from
openinga.pcx (the title screen; the title's lettering is left out). The result is derived from
the game's art: like the packaged app it is for private use and is never committed.

Standard library only (the build has no imaging package): an 8-bit PCX reader, a PNG writer and
the few image operations needed.
"""
import struct
import sys
import zlib
from pathlib import Path

SOURCE = "openinga.pcx"
SIZE = 1024  # the icon canvas
BODY = 824  # the rounded square inside it (Apple's icon grid)
RADIUS = 185.4  # its corner radius

# Source rectangles in openinga.pcx (1024 x 768), as (left, top, right, bottom).
PLANET = (384, 0, 1024, 640)  # the square the icon shows
LETTERING = (0, 420, 620, 768)  # "SID MEIER'S ALPHA CENTAURI": blacked out
SUNS = (20, 0, 460, 300)  # the two suns and their haze, left of the square
SUNS_AT = (-80, 16)  # where they go in the square (lightened onto it)
EDGE = (58, 66, 84)  # a faint rim, so that the shape shows on a dark background


def read_pcx(path):
    """An 8-bit, one-plane PCX as (width, height, rows of palette indices, palette bytes)."""
    data = Path(path).read_bytes()
    if len(data) < 128 + 769 or data[0] != 0x0A or data[3] != 8 or data[65] != 1 or data[-769] != 0x0C:
        raise ValueError(f"{path}: not an 8-bit PCX with a palette")
    x0, y0, x1, y1 = struct.unpack_from("<HHHH", data, 4)
    width, height = x1 - x0 + 1, y1 - y0 + 1
    line = struct.unpack_from("<H", data, 66)[0]
    out = bytearray()
    i, end = 128, len(data) - 769
    while len(out) < line * height and i < end:
        b = data[i]
        i += 1
        if b >= 0xC0:
            out += bytes([data[i]]) * (b & 0x3F)
            i += 1
        else:
            out.append(b)
    if len(out) < line * height:
        raise ValueError(f"{path}: picture data ends early")
    rows = [out[y * line : y * line + width] for y in range(height)]
    return width, height, rows, data[-768:]


def write_png(path, width, height, rgba):
    """rgba: height rows of width * 4 bytes."""

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    raw = b"".join(b"\0" + bytes(row) for row in rgba)
    Path(path).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def compose(rows, palette):
    """The square picture as rows of (r, g, b) tuples."""
    rgb = [palette[i * 3 : i * 3 + 3] for i in range(256)]
    left, top, right, bottom = PLANET
    lx0, ly0, lx1, ly1 = LETTERING
    square = []
    for y in range(top, bottom):
        row = rows[y]
        square.append(
            [(0, 0, 0) if lx0 <= x < lx1 and ly0 <= y < ly1 else tuple(rgb[row[x]]) for x in range(left, right)]
        )
    # The suns: each pixel the lighter of the two pictures, fading out from the patch's centre to
    # nothing at its edge (an ellipse), so that the patch's outline does not show in the haze.
    sx0, sy0, sx1, sy1 = SUNS
    ax, ay = SUNS_AT
    w, h = sx1 - sx0, sy1 - sy0
    for y in range(h):
        ty = ay + y
        if not 0 <= ty < len(square):
            continue
        src = rows[sy0 + y]
        for x in range(w):
            tx = ax + x
            if not 0 <= tx < len(square[ty]):
                continue
            r = (((x + 0.5) / w * 2 - 1) ** 2 + ((y + 0.5) / h * 2 - 1) ** 2) ** 0.5
            k = max(0.0, 1.0 - r * r) ** 2 if r < 1.0 else 0.0
            k = min(1.0, k * 2.2)  # the suns themselves at full strength
            s = rgb[src[sx0 + x]]
            d = square[ty][tx]
            square[ty][tx] = tuple(max(d[c], int(s[c] * k)) for c in range(3))
    return square


def outside(x, y):
    """The distance of the pixel at (x, y) from the rounded square's outline: negative inside."""
    half = BODY / 2
    px, py = abs(x + 0.5 - SIZE / 2), abs(y + 0.5 - SIZE / 2)
    qx, qy = px - (half - RADIUS), py - (half - RADIUS)
    return (max(qx, 0) ** 2 + max(qy, 0) ** 2) ** 0.5 + min(max(qx, qy), 0) - RADIUS


def render(square):
    """The icon: the square scaled (bilinear) into the rounded body on a transparent canvas."""
    n = len(square)
    margin = (SIZE - BODY) / 2
    scale = n / BODY
    out = []
    for y in range(SIZE):
        row = bytearray(SIZE * 4)
        fy = (y + 0.5 - margin) * scale - 0.5
        y0 = min(max(int(fy), 0), n - 1)
        y1 = min(y0 + 1, n - 1)
        wy = min(max(fy - y0, 0.0), 1.0)
        top, bottom = square[y0], square[y1]
        for x in range(SIZE):
            d = outside(x, y)
            a = max(0.0, min(1.0, 0.5 - d))  # how much of the pixel is inside
            if a <= 0.0:
                continue
            rim = max(0.0, min(1.0, (d + 5.0) / 5.0)) * 0.7  # the last five pixels before the outline
            fx = (x + 0.5 - margin) * scale - 0.5
            x0 = min(max(int(fx), 0), n - 1)
            x1 = min(x0 + 1, n - 1)
            wx = min(max(fx - x0, 0.0), 1.0)
            p00, p01, p10, p11 = top[x0], top[x1], bottom[x0], bottom[x1]
            o = x * 4
            for c in range(3):
                upper = p00[c] + (p01[c] - p00[c]) * wx
                lower = p10[c] + (p11[c] - p10[c]) * wx
                v = upper + (lower - upper) * wy
                row[o + c] = int(v + (max(v, EDGE[c]) - v) * rim + 0.5)
            row[o + 3] = int(a * 255 + 0.5)
        out.append(row)
    return out


def find(game_dir, name):
    """The game's file of that name, whatever its case."""
    for p in Path(game_dir).iterdir():
        if p.name.lower() == name:
            return p
    raise FileNotFoundError(f"{name} not found in {game_dir}")


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2].strip(), file=sys.stderr)
        return 2
    width, height, rows, palette = read_pcx(find(argv[1], SOURCE))
    if (width, height) != (1024, 768):
        raise ValueError(f"{SOURCE} is {width} x {height}, not the 1024 x 768 title screen")
    write_png(argv[2], SIZE, SIZE, render(compose(rows, palette)))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except (OSError, ValueError) as e:
        print(f"make_icon: {e}", file=sys.stderr)
        sys.exit(1)

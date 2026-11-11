"""Extract two existing same-slot/same-LOD MOD3 meshes for the local DX12 fixture.

Input files are read only. The output contains game geometry and stays local.
Run from the repository root; pass output under build/ or another scratch folder.
"""
import argparse
import array
import hashlib
import math
from pathlib import Path
import struct
import sys


def mesh_geometry(data, section_offsets, index):
    table, vertices, indices = section_offsets
    fields = struct.unpack_from("<7H2B6I", data, table + 80 * index)
    count, slot, lod, stride = fields[1], fields[3], fields[4], fields[7]
    sub, offset, _, first, length, base = fields[9:]
    if count < 3 or length < 3 or length % 3 or stride < 12:
        raise ValueError("No indexed triangle geometry in selected row")
    vertex_base = vertices + offset + stride * (sub + base)
    if vertex_base + stride * (count - 1) + 12 > indices or indices + 2 * (first + length) > len(data):
        raise ValueError("Selected mesh exceeds its vertex or index buffer")
    points = [struct.unpack_from("<3f", data, vertex_base + stride * i) for i in range(count)]
    if not all(math.isfinite(v) for point in points for v in point):
        raise ValueError("Non-finite vertex positions")
    faces = [v - sub for v, in struct.iter_unpack("<H", memoryview(data)[indices + first * 2:indices + (first + length) * 2])]
    if len(faces) != length or min(faces) < 0 or max(faces) >= count:
        raise ValueError("Invalid triangle index range")
    return slot, lod, points, faces


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("model", type=Path)
    ap.add_argument("target", type=int)
    ap.add_argument("sibling", type=int)
    ap.add_argument("output", type=Path)
    ap.add_argument("--preset-pair", action="store_true", help="Allow a preset handoff between different material slots")
    args = ap.parse_args()
    data = args.model.read_bytes()
    if len(data) < 0x60 or data[:6] != b"MOD\0\xed\0":
        ap.error("Expected World MOD3 version 237")
    count, slots = struct.unpack_from("<HH", data, 8)
    if args.target == args.sibling or not all(0 <= i < count for i in (args.target, args.sibling)):
        ap.error("Choose two different valid mesh rows")
    name_table, *offsets = struct.unpack_from("<4Q", data, 0x40)
    meshes, vertices, indices = offsets
    if not (name_table + 128 * slots <= len(data) and meshes + 80 * count <= vertices <= indices <= len(data)):
        ap.error("Invalid MOD3 section bounds")
    try:
        a, b = [mesh_geometry(data, offsets, i) for i in (args.target, args.sibling)]
    except ValueError as error:
        ap.error(str(error))
    if a[1] != b[1] or max(a[0], b[0]) >= slots or (a[0] != b[0] and not args.preset_pair):
        ap.error("Choose valid rows sharing a LOD and, for isolation tests, a material slot")
    name_start = name_table + 128 * a[0]
    material = data[name_start:name_start + 128].split(b"\0")[0].decode("ascii", "replace")
    points = a[2] + b[2]
    low = [min(p[i] for p in points) for i in range(3)]
    high = [max(p[i] for p in points) for i in range(3)]
    span = max(high[0] - low[0], high[1] - low[1])
    h0 = min(p[1] for p in a[2])
    height = max(p[1] for p in a[2]) - h0
    if not span or not height:
        ap.error("Fixture requires nonzero XY extent and target height")
    vb = array.array("f")
    for p in points:
        vb.extend((1.8 * (p[0] - (low[0] + high[0]) / 2) / span,
                   1.8 * (p[1] - (low[1] + high[1]) / 2) / span,
                   0.1 + 0.8 * (p[2] - low[2]) / max(high[2] - low[2], 0.00001), *p))
    ib = array.array("I", a[3] + [len(a[2]) + i for i in b[3]])
    if sys.byteorder != "little":
        ib.byteswap()
        vb.byteswap()
    header = struct.pack("<4sIIIff", b"WDM1", len(points), len(a[3]), len(b[3]), h0, 1.0 / height)
    if args.output.resolve() == args.model.resolve():
        ap.error("Fixture output must be separate from the input model")
    args.output.write_bytes(header + vb.tobytes() + ib.tobytes())
    print(f"rows={args.target},{args.sibling} slots={a[0]},{b[0]} target={material} lod={a[1]} "
          f"triangles={len(a[3])//3},{len(b[3])//3} source_sha256={hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()

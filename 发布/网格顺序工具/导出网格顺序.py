"""Export Blender Outliner name order without rewriting MOD3s or mesh IDs.

In Blender's Python console:
    exec(compile(open(SCRIPT, encoding='utf-8').read(), SCRIPT, 'exec'))
    export_current_scene(GAME_ROOT)

Offline:
    python export_mesh_order.py --snapshot scene.json --game-root GAME_ROOT
        --output-root OUTPUT_ROOT
"""
import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path


def capture_scene():
    import bpy

    def collection_record(collection):
        return {
            'name': collection.name,
            'objects': [o.name for o in collection.all_objects if o.type == 'MESH'],
            'children': [collection_record(c) for c in collection.children],
        }

    return {
        'file': bpy.data.filepath,
        'meshes': [{'name': o.name} for o in bpy.data.objects if o.type == 'MESH'],
        'scene_collections': collection_record(bpy.context.scene.collection),
    }


def natural_key(name):
    return tuple(int(part) if part.isdecimal() else part.casefold()
                 for part in re.split(r'(\d+)', name))


def read_mod3(path):
    data = path.read_bytes()
    if data[:4] != b'MOD\0' or struct.unpack_from('<H', data, 4)[0] != 237:
        raise ValueError(f'Unsupported MOD3: {path}')
    mesh_count, material_count = struct.unpack_from('<HH', data, 8)
    material_at, mesh_at = struct.unpack_from('<QQ', data, 0x40)
    if material_at + material_count * 128 > len(data) or mesh_at + mesh_count * 80 > len(data):
        raise ValueError(f'Truncated MOD3: {path}')
    names = [data[material_at + i*128:material_at + (i+1)*128].split(b'\0')[0]
             for i in range(material_count)]
    table = data[mesh_at:mesh_at + mesh_count*80]
    signature = 14695981039346656037
    for byte in b''.join(name + b'\0' for name in names) + table:
        signature = ((signature ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    materials = [names[struct.unpack_from('<H', table, i*80 + 6)[0]].decode('utf-8')
                 for i in range(mesh_count)]
    return signature, materials


def export_mesh_order(snapshot, game_root, output_root=None):
    game_root = Path(game_root)
    output_root = Path(output_root) if output_root is not None else game_root
    collections = defaultdict(list)

    def visit(collection):
        if collection['name'].lower().endswith('.mod3'):
            # Older read-only snapshots store direct objects at each level.
            def objects_under(c):
                return list(dict.fromkeys(c['objects'] + [n for child in c['children']
                                                         for n in objects_under(child)]))
            collections[collection['name'].lower()].append(objects_under(collection))
        for child in collection['children']:
            visit(child)

    visit(snapshot['scene_collections'])
    report = []
    for sex in ('f_equip', 'm_equip'):
        folder = game_root / 'nativePC' / 'pl' / sex
        for path in sorted(folder.glob('*/*/mod/*.mod3')):
            candidates = collections.get(path.name.lower(), [])
            if not candidates:
                continue
            if len(candidates) != 1:
                raise ValueError(f'Ambiguous Blender model collection: {path.name}')
            signature, materials = read_mod3(path)
            by_material = defaultdict(list)
            for name in candidates[0]:
                if '__' not in name:
                    raise ValueError(f'Object has no exported material-name suffix: {name}')
                by_material[name.split('__', 1)[1]].append(name)
            if len(candidates[0]) != len(materials):
                raise ValueError(f'Mesh count differs from Blender: {path.name}')
            # The same material may belong to multiple meshes. Only accept that
            # case when the complete collection order matches the exported table.
            collection_materials = [name.split('__', 1)[1] for name in candidates[0]]
            matching_sequence = collection_materials == materials
            used = defaultdict(int)
            records = []
            for index, material in enumerate(materials):
                names = by_material[material]
                if not names or (len(names) > 1 and not matching_sequence):
                    raise ValueError(f'Cannot uniquely bind mesh {index}: {material}')
                occurrence = used[material]
                if occurrence >= len(names):
                    raise ValueError(f'Material mesh count differs: {material}')
                records.append({'index': index, 'material': material, 'object': names[occurrence]})
                used[material] += 1
            records.sort(key=lambda row: natural_key(row['object']))
            destination = output_root / path.relative_to(game_root).with_suffix('.meshorder')
            lines = ['WorldDiffMeshOrder 1', f'SIGNATURE {signature}']
            # std::quoted accepts backslash escapes for quotes/backslashes, but
            # not JSON \u escapes: preserve UTF-8 bytes verbatim.
            def quote(value):
                if any(ord(c) < 32 for c in value):
                    raise ValueError('Control character in mesh name')
                return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'
            lines += [f'MESH {r["index"]} {quote(r["material"])} {quote(r["object"])}' for r in records]
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text('\n'.join(lines) + '\n', encoding='utf-8')
            report.append({'model': str(path), 'sidecar': str(destination),
                           'signature': signature, 'meshes': records})
    if not report:
        raise ValueError('No matching .mod3 collections found in this scene')
    return report


def export_current_scene(game_root, output_root=None):
    report = export_mesh_order(capture_scene(), game_root, output_root)
    print('WorldDiff:', len(report), 'model order tables exported')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', required=True, type=Path)
    parser.add_argument('--game-root', required=True, type=Path)
    parser.add_argument('--output-root', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    report = export_mesh_order(json.loads(args.snapshot.read_text(encoding='utf-8')),
                              args.game_root, args.output_root)
    if args.report:
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'Exported {len(report)} models, {sum(len(r["meshes"]) for r in report)} meshes')


if __name__ == '__main__' and 'bpy' not in sys.modules:
    main()

"""Summarize one WorldDiff read-only dissolve probe capture."""
import argparse
import csv
import json
import math
from collections import Counter, defaultdict
from pathlib import Path


def summarize(path):
    totals = Counter()
    groups = defaultdict(lambda: [[], []])
    dropped = None
    with path.open(encoding="utf-8-sig", newline="") as stream:
        header = next(csv.reader([stream.readline()]))
        for line in stream:
            if line.startswith("# dropped="):
                dropped = int(line.split("=", 1)[1])
                continue
            row = next(csv.DictReader([line], fieldnames=header))
            totals["samples"] += 1
            if row["readable"] != "1":
                totals["unreadable"] += 1
                continue
            if row["mesh_ref_matches"] != "1" or int(row["packet_index"]) != int(row["mesh"]):
                totals["identity_mismatch"] += 1
                continue
            model, packet = float(row["model_314"]), float(row["packet_64"])
            if not (math.isfinite(model) and math.isfinite(packet)):
                totals["nonfinite"] += 1
                continue
            key = (row["instance"], int(row["part"]), int(row["mesh"]), int(row["material"]))
            values, candidates = groups[key]
            values.append(model)
            candidates.append(packet)
            totals["usable"] += 1
    return totals, groups, dropped


def write_pair(capture, output, instance, part, rows):
    """Join observed rows by one model and Present tick, preserving capture order."""
    if rows[0] == rows[1] or capture.resolve() == output.resolve():
        raise ValueError("Select different rows and a separate output file")
    samples = defaultdict(lambda: defaultdict(set))
    signatures = defaultdict(set)
    dropped = None
    with capture.open(encoding="utf-8-sig", newline="") as stream:
        header = next(csv.reader([stream.readline()]))
        for line in stream:
            if line.startswith("# dropped="):
                dropped = int(line.split("=", 1)[1])
                continue
            record = next(csv.DictReader([line], fieldnames=header))
            index = int(record["mesh"])
            if (record["instance"] != instance or int(record["part"]) != part or index not in rows
                    or record["readable"] != "1" or record["mesh_ref_matches"] != "1"
                    or int(record["packet_index"]) != index):
                continue
            phase = float(record["planned_hidden"])
            tick = int(record["present_tick"])
            if tick and 0 <= phase <= 1:
                signatures[index].add(tuple(record[field] for field in
                                      ("material", "first_index", "index_count", "base_vertex")))
                samples[tick][index].add((phase, int(record["original"]), int(record["resolved"])))
    if dropped is None or any(len(signatures[i]) != 1 for i in rows):
        raise ValueError("Capture is incomplete, missing a row, or contains multiple draw ranges for a row")
    pairs = []
    for tick, group in samples.items():
        if all(len(group.get(i, ())) == 1 for i in rows):
            pairs.append((tick, *next(iter(group[rows[0]])), *next(iter(group[rows[1]]))))
    if not pairs:
        raise ValueError("No complete unambiguous paired ticks")
    output.write_text("".join(" ".join(map(str, pair)) + "\n" for pair in pairs), encoding="ascii")
    print("paired ticks", len(pairs), "unpaired or ambiguous", len(samples) - len(pairs),
          "dropped capture requests", dropped)


def summarize_scene(path, part, rows):
    """Check complementary submitted index counts for one observed pair."""
    if rows[0] == rows[1]:
        raise ValueError("Choose two different meshes")
    frames = {}
    owners = set()
    passes = {row: set() for row in rows}
    footer = None
    with path.open(encoding="utf-8-sig", newline="") as stream:
        header = next(csv.reader([stream.readline()]))
        for line in stream:
            if line.startswith("#"):
                footer = line.strip()
                continue
            r = next(csv.DictReader([line], fieldnames=header))
            if int(r["part"]) != part or int(r["mesh"]) not in passes:
                continue
            frame = int(r["handoff_frame"])
            if not frame:
                continue
            row = int(r["mesh"])
            total, cut, submitted = (int(r[k]) for k in ("index_count","cut_indices","submitted_indices"))
            expected = total - cut if row == rows[0] else cut
            if not (0 <= cut <= total and cut % 3 == 0 and submitted == expected):
                raise ValueError(f"Invalid partition at frame {frame}, row {row}")
            owners.add((r["instance"],r["ib"],r["ib_bytes"]))
            stage = (int(r["color_targets"]),r["pso"],r["depth_view"])
            passes[row].add(stage)
            sample = frames.setdefault(frame, (cut,total,set(),int(r["tick"])))
            if sample[:2] != (cut,total):
                raise ValueError(f"Conflicting progress at frame {frame}")
            sample[2].add((row,stage))
    if not footer or not footer.startswith("# visited=") or len(owners) != 1 or not frames:
        raise ValueError("Incomplete capture or no unique prepared instance")
    expected_stages = passes[rows[0]]
    if expected_stages != passes[rows[1]] or sorted(s[0] for s in expected_stages) != [0,0,6]:
        raise ValueError("The two rows did not visit the same three stages")
    expected_draws = {(row,s) for row in rows for s in expected_stages}
    ordered = sorted(frames.items())
    mid = [(f,c,t,s) for f,(c,t,s,_) in ordered if 0<c<t]
    complete = sum(s == expected_draws for _,_,_,s in mid)
    cuts = [c for _,(c,_,_,_) in ordered]
    intervals = []
    start_tick = last_tick = None
    for _,(cut,total,_,tick) in ordered:
        if 0 < cut < total:
            if start_tick is None:
                start_tick = tick
            last_tick = tick
        elif start_tick is not None:
            intervals.append((last_tick-start_tick) & 0xffffffff)
            start_tick = None
    if start_tick is not None:
        intervals.append((last_tick-start_tick) & 0xffffffff)
    info = dict(instance=next(iter(owners))[0], part=part, rows=rows,
                observed_snapshots=len(frames), intermediate_snapshots=len(mid),
                intermediate_with_all_six_draws=complete,
                increasing_steps=sum(b>a for a,b in zip(cuts,cuts[1:])),
                decreasing_steps=sum(b<a for a,b in zip(cuts,cuts[1:])),
                observed_intermediate_spans_ms=intervals,
                first_cut=cuts[0],last_cut=cuts[-1],index_count=ordered[0][1][1],footer=footer)
    return info


def summarize_universal(path):
    """Single-row show/hide, no expected outfit, mesh number, or partner."""
    samples = defaultdict(dict)
    stages = defaultdict(set)
    footer = None
    with path.open(encoding="utf-8-sig", newline="") as stream:
        header = next(csv.reader([stream.readline()]))
        for line in stream:
            if line.startswith("#"):
                footer = line.strip()
                continue
            r = next(csv.DictReader([line],fieldnames=header))
            if r["handoff_frame"]=="0":
                continue
            key = (r["instance"],int(r["part"]),int(r["mesh"]),int(r["index_count"]))
            submitted = int(r["submitted_indices"])
            if not 0<=submitted<=key[3] or submitted%3:
                raise ValueError(f"Invalid native triangle count for {key}")
            tick = int(r["handoff_frame"])
            samples[key].setdefault(tick,set()).add(submitted)
            if 0<submitted<key[3]:
                stages[key].add((int(r["color_targets"]),r["pso"],r["depth_view"]))
    if not footer or not footer.startswith("# visited="):
        raise ValueError("Incomplete scene capture")
    report = []
    for key,counts in sorted(samples.items()):
        ms = sorted(counts)
        if not stages[key]:
            continue
        # Different recording workers may observe the same millisecond. Only
        # unambiguous progress samples are used for directional comparisons.
        values = [next(iter(counts[t])) for t in ms if len(counts[t])==1]
        intervals = []
        for t in ms:
            if not any(0<v<key[3] for v in counts[t]):
                continue
            if not intervals or t-intervals[-1][-1]>250: intervals.append([])
            intervals[-1].append(t)
        report.append(dict(instance=key[0],part=key[1],mesh=key[2],index_count=key[3],
                           increasing=sum(b>a for a,b in zip(values,values[1:])),
                           decreasing=sum(b<a for a,b in zip(values,values[1:])),
                           observed_intermediate_ms=[group[-1]-group[0] for group in intervals],
                           color_target_counts=sorted(s[0] for s in stages[key])))
    return dict(footer=footer,rows=report)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--universal-scene", action="store_true", help="Audit any independently appearing/disappearing rows")
    parser.add_argument("--handoff-scene", type=int, nargs=3, metavar=("PART","FIRST","SECOND"),
                        help="Audit paired geometry on the native scene list")
    parser.add_argument("--pair", type=int, nargs=3, metavar=("PART", "FIRST", "SECOND"))
    parser.add_argument("--instance", help="Model instance printed by the probe")
    parser.add_argument("--replay-out", type=Path, help="Write matched ticks for the DX12 handoff test")
    args = parser.parse_args()
    if args.universal_scene:
        if args.handoff_scene or args.pair or args.instance or args.replay_out:
            parser.error("--universal-scene takes only a scene CSV")
        info=summarize_universal(args.capture)
        print(json.dumps(info,indent=2))
        return 0 if info["rows"] else 1
    if args.handoff_scene is not None:
        if args.pair is not None or args.instance or args.replay_out:
            parser.error("--handoff-scene uses a scene CSV and takes no query options")
        part,first,second=args.handoff_scene
        info=summarize_scene(args.capture,part,[first,second])
        print(json.dumps(info,indent=2))
        return 0 if info["intermediate_with_all_six_draws"] else 1
    if args.pair is not None or args.instance is not None or args.replay_out is not None:
        if args.pair is None or not args.instance or not args.replay_out:
            parser.error("--pair, --instance and --replay-out must be used together")
        write_pair(args.capture, args.replay_out, args.instance, args.pair[0], args.pair[1:])
        return 0
    report, meshes, dropped = summarize(args.capture)
    print("samples", dict(report), "dropped", dropped if dropped is not None else "incomplete capture")
    print("instance part mesh material count model_range packet_range near_zero partial near_one matching_values")
    for key, (model, packet) in sorted(meshes.items()):
        print(*key, len(model), (min(model), max(model)), (min(packet), max(packet)),
              sum(abs(v) <= 0.0001 for v in model),
              sum(0.0001 < v < 0.9999 for v in model),
              sum(abs(v - 1) <= 0.0001 for v in model),
              sum(abs(a - b) <= 0.0001 for a, b in zip(model, packet)))
    return 0 if meshes and dropped is not None else 1


if __name__ == "__main__":
    raise SystemExit(main())

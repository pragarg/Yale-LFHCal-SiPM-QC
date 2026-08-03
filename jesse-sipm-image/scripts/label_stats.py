#!/usr/bin/env python3
"""Summarize SiPM label JSON exports.

The report includes pristine/damaged counts, per-grade area statistics, artifact
mask counts, discrepancy checks, and a CSV for plotting.
"""

import argparse, glob, json, os, statistics, sys
from collections import Counter, defaultdict


def pctile(xs, p):
    if not xs:
        return float("nan")
    xs = sorted(xs)
    if len(xs) == 1:
        return xs[0]
    i = (len(xs) - 1) * p
    lo, hi = int(i), min(int(i) + 1, len(xs) - 1)
    return xs[lo] + (xs[hi] - xs[lo]) * (i - lo)


def load(paths):
    imgs, per_tray = [], defaultdict(list)
    seen = {}
    for p in paths:
        try:
            d = json.load(open(p))
        except Exception as e:
            print(f"!! could not read {p}: {e}", file=sys.stderr)
            continue
        tray = os.path.basename(p)
        for im in d.get("images", []):
            key = im.get("key") or im.get("filename")
            if key in seen:
                print(f"   (dup key {key} — using the later file)", file=sys.stderr)
            seen[key] = im
            im["_tray"] = tray
        # Keep the source grouping for the per-tray summary.
        for im in d.get("images", []):
            per_tray[tray].append(im)
    imgs = list(seen.values())
    return imgs, per_tray


def chip_issues(i):
    t = set(i.get("types") or [])
    real = t & {"scratch", "bubble"}
    red = (i.get("damagePixels") or 0) > 0
    out = []
    if i["status"] == "pristine":
        if real: out.append("pristine+realtype")
        if red:  out.append("pristine+red")
    if i["status"] == "damaged":
        if i["grade"] is None: out.append("damaged+nograde")
        elif i["grade"] == 0:  out.append("damaged+grade0")
        if not t:              out.append("damaged+notype")
        if i["grade"] and i["grade"] >= 1 and not red: out.append("graded+nored")
    return out


def area_pct(i):
    af = i.get("affectedFraction")
    return (af * 100.0) if af else 0.0


def main():
    ap = argparse.ArgumentParser(description="Aggregate SiPM label exports into stats.")
    ap.add_argument("files", nargs="*", help="label JSON files")
    ap.add_argument("--dir", help="folder of *.json to include")
    ap.add_argument("--csv", default="per_image_stats.csv", help="output CSV path")
    args = ap.parse_args()

    paths = list(args.files)
    if args.dir:
        paths += sorted(glob.glob(os.path.join(args.dir, "*.json")))
    if not paths:
        sys.exit("Give me some label JSON files (or --dir folder). See --help.")

    imgs, per_tray = load(paths)
    if not imgs:
        sys.exit("No images found in those files.")

    N = len(imgs)
    pristine = [i for i in imgs if i["status"] == "pristine"]
    damaged  = [i for i in imgs if i["status"] == "damaged"]
    by_grade = defaultdict(list)
    for i in imgs:
        by_grade[i.get("grade")].append(i)

    line = "=" * 64
    print(line)
    print(f" SiPM DATASET STATS   ({len(paths)} file(s), {N} unique chips)")
    print(line)
    print(f"  pristine : {len(pristine):5d}  ({len(pristine)/N*100:4.1f}%)")
    print(f"  damaged  : {len(damaged):5d}  ({len(damaged)/N*100:4.1f}%)")

    #pristine chips with artifact masks.
    hard_neg = [i for i in pristine if (i.get("falsePixels") or 0) > 0]
    print(f"  hard negatives (pristine WITH a false/artifact mask): {len(hard_neg)}")

    print("\n--- per-grade counts ---")
    for g in [0, 1, 2, 3, 4, 5]:
        c = len(by_grade.get(g, []))
        bar = "#" * min(50, c)
        print(f"  grade {g}: {c:5d}  {bar}")
    nullg = len(by_grade.get(None, []))
    if nullg:
        print(f"  grade ?: {nullg:5d}  (no grade set — fix these)")

    #Area distribution by damage grade.
    print("\n--- painted damage area as % of image, per grade ---")
    print(f"  {'grade':>5} {'n':>4} {'min':>7} {'p25':>7} {'median':>7} {'p75':>7} {'max':>7}")
    medians = {}
    for g in [1, 2, 3, 4, 5]:
        xs = [area_pct(i) for i in by_grade.get(g, []) if i["status"] == "damaged"]
        if not xs:
            print(f"  {g:>5} {0:>4}      —       —       —       —       —")
            continue
        medians[g] = statistics.median(xs)
        print(f"  {g:>5} {len(xs):>4} {min(xs):>7.2f} {pctile(xs,.25):>7.2f} "
              f"{statistics.median(xs):>7.2f} {pctile(xs,.75):>7.2f} {max(xs):>7.2f}")

    # Suggested cutoffs are midpoints between adjacent grade medians.
    print("\n--- suggested grade cutoffs (from YOUR data) ---")
    gs = [g for g in [1, 2, 3, 4, 5] if g in medians]
    if len(gs) >= 2:
        print("  boundary between grades = midpoint of their median areas:")
        for a, b in zip(gs, gs[1:]):
            cut = (medians[a] + medians[b]) / 2
            overlap = ""
            xa = [area_pct(i) for i in by_grade.get(a, [])]
            xb = [area_pct(i) for i in by_grade.get(b, [])]
            if xa and xb and max(xa) > min(xb):
                overlap = "  (ranges overlap — boundary is fuzzy, expected)"
            print(f"    {a} | {b}  ≈ {cut:6.2f}% {overlap}")
        print("  -> use these as a starting point; review the high-grade boundaries.")
    else:
        print("  not enough graded data yet to suggest cutoffs.")

    # Damage type counts.
    print("\n--- damage types (on damaged chips) ---")
    tc = Counter()
    for i in damaged:
        for t in (i.get("types") or []):
            tc[t] += 1
    for t in ["scratch", "bubble", "false"]:
        print(f"  {t:8}: {tc.get(t,0)}")

    # Discrepancy checks.
    flagged = [(i["filename"], chip_issues(i)) for i in imgs if chip_issues(i)]
    print(f"\n--- discrepancy audit ---")
    print(f"  flagged chips: {len(flagged)}")
    for nm, iss in flagged[:20]:
        print("   ", nm, iss)
    if len(flagged) > 20:
        print(f"    ... +{len(flagged)-20} more")

    # Per-tray breakdown.
    print("\n--- per-tray breakdown ---")
    for tray, ims in per_tray.items():
        d = sum(1 for i in ims if i["status"] == "damaged")
        gd = Counter(i.get("grade") for i in ims if i["status"] == "damaged")
        gstr = " ".join(f"G{g}:{gd[g]}" for g in [1,2,3,4,5] if gd.get(g))
        print(f"  {tray}: {len(ims)} chips, {d} damaged   {gstr}")

    # CSV for plotting.
    with open(args.csv, "w") as f:
        f.write("tray,filename,status,grade,area_pct,damagePixels,falsePixels\n")
        for i in imgs:
            f.write(f'{i.get("_tray","")},{i["filename"]},{i["status"]},'
                    f'{i.get("grade")},{area_pct(i):.4f},'
                    f'{i.get("damagePixels",0)},{i.get("falsePixels",0)}\n')
    print(f"\nWrote {args.csv} ({N} rows) for plotting.")

    #compact summary.
    print("\n" + line)
    print(" LABEL SUMMARY")
    print(line)
    gc = {g: len(by_grade.get(g, [])) for g in [1,2,3,4,5]}
    print(f"  total images: {N}")
    print(f"  # damaged: {len(damaged)}   # pristine: {len(pristine)}")
    print(f"  per-grade: G1:{gc[1]} G2:{gc[2]} G3:{gc[3]} G4:{gc[4]} G5:{gc[5]}")
    print(f"  pristine-with-artifact (hard negatives): {len(hard_neg)}")
    print("  painted-% per grade (median): " +
          ", ".join(f"G{g}:{medians[g]:.2f}%" for g in gs))


if __name__ == "__main__":
    main()

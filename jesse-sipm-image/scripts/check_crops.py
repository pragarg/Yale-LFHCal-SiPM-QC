#!/usr/bin/env python3
"""Check cropped SiPM images for unusual size or aspect ratio.

The script scans cropped tray folders, prints a report, and writes bad_crops.csv.
It does not modify image files.
"""
import argparse, csv, os, sys, statistics
from collections import defaultdict

try:
    from PIL import Image
except ImportError:
    sys.exit("Needs Pillow:  pip3 install pillow")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cropped", default="cropped")
    ap.add_argument("--max-ar", type=float, default=1.30,
                    help="flag crops more rectangular than this (default 1.30)")
    ap.add_argument("--out", default="bad_crops.csv")
    args = ap.parse_args()

    if not os.path.isdir(args.cropped):
        sys.exit(f"No '{args.cropped}' folder found. Run from the project folder.")

    trays = sorted(d for d in os.listdir(args.cropped)
                   if os.path.isdir(os.path.join(args.cropped, d)))
    if not trays:
        sys.exit(f"No tray folders inside '{args.cropped}'.")

    flagged = []
    total = 0
    print(f"scanning {len(trays)} trays in '{args.cropped}'...\n")

    for tray in trays:
        td = os.path.join(args.cropped, tray)
        files = sorted(f for f in os.listdir(td) if f.lower().endswith(".png"))
        sizes = {}
        for fn in files:
            try:
                w, h = Image.open(os.path.join(td, fn)).size
                sizes[fn] = (w, h)
            except Exception as e:
                flagged.append((tray, fn, "unreadable", str(e)))
        if not sizes:
            continue
        total += len(sizes)

        #typical crop size for this tray.
        big = [max(w, h) for (w, h) in sizes.values()]
        med_big = statistics.median(big)

        for fn, (w, h) in sizes.items():
            ar = max(w, h) / max(1, min(w, h))
            reasons = []
            if ar > args.max_ar:
                reasons.append(f"rectangular(ar={ar:.2f})")
            # Flag large or small size outliers.
            if max(w, h) > med_big * 1.4:
                reasons.append(f"too_big({w}x{h} vs ~{int(med_big)})")
            if max(w, h) < med_big * 0.6:
                reasons.append(f"too_small({w}x{h} vs ~{int(med_big)})")
            if reasons:
                flagged.append((tray, fn, ";".join(reasons), f"{w}x{h}"))

    # Print and save the report.
    print(f"checked {total} crops across {len(trays)} trays")
    print(f"FLAGGED: {len(flagged)}\n")
    by_tray = defaultdict(list)
    for tray, fn, why, size in flagged:
        by_tray[tray].append((fn, why, size))
    for tray in sorted(by_tray):
        print(f"  {tray}:  ({len(by_tray[tray])} to check)")
        for fn, why, size in by_tray[tray]:
            print(f"      {fn:22} {size:>10}  {why}")

    with open(args.out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["tray", "chip", "size", "reason"])
        for tray, fn, why, size in flagged:
            w.writerow([tray, fn, size, why])
    print(f"\nwrote {args.out}")
    if flagged:
        print("\nReview the flagged crops and re-crop them if needed.")
        print("Example single re-crop:")
        print('  python3 prep_sipm_crops.py --in raw/<tray> --out cropped/<tray> \\')
        print('        --debug debug/<tray> --inner --inner-thresh 150')
    else:
        print("\nAll crops look square/proportionate. Nothing to fix.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
count_photos.py  —  count trays and SiPM photos in a folder.

USAGE (from your project folder):
    python3 count_photos.py                 # counts cropped/ by default
    python3 count_photos.py --dir raw       # count the raw photos instead
    python3 count_photos.py --dir cropped --ext .png

Counts one level of tray subfolders and the image files inside each.
"""
import argparse, os, sys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default="cropped", help="folder of tray subfolders (default: cropped)")
    ap.add_argument("--ext", default=".png", help="image extension to count (default: .png)")
    args = ap.parse_args()

    if not os.path.isdir(args.dir):
        sys.exit(f"No '{args.dir}' folder here. Run from your project folder, or pass --dir.")

    ext = args.ext.lower()
    trays = sorted(d for d in os.listdir(args.dir)
                   if os.path.isdir(os.path.join(args.dir, d)))
    if not trays:
        sys.exit(f"No tray subfolders inside '{args.dir}'.")

    total = 0
    counts = []
    for t in trays:
        n = sum(1 for f in os.listdir(os.path.join(args.dir, t))
                if f.lower().endswith(ext))
        counts.append((t, n)); total += n

    print(f"Folder: {args.dir}/   (counting {ext} files)\n")
    print(f"{'tray':18} {'photos':>7}")
    print("-" * 27)
    for t, n in counts:
        print(f"{t:18} {n:>7}")
    print("-" * 27)
    print(f"{'TOTAL':18} {total:>7}")
    print(f"\nTrays: {len(trays)}   |   Photos: {total}", end="")
    if trays:
        avg = total / len(trays)
        print(f"   |   avg {avg:.0f}/tray")
        odd = [t for t, n in counts if abs(n - 460) > 5]
        if odd:
            print(f"\nNote: {len(odd)} tray(s) not ~460 photos: {', '.join(odd)}")

if __name__ == "__main__":
    main()

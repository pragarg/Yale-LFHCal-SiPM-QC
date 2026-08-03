#!/usr/bin/env python3



import argparse, csv, os, sys
import numpy as np
import torch
from PIL import Image
import torchvision.transforms.functional as TF

SIZE = 512 #image size for the model input (512x512)
THRESHOLD = 300     # cutoff for the binary predicted_damaged flag


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="sipm_unet_final_3tray.pt")
    ap.add_argument("--cropped", default="cropped")
    ap.add_argument("--out", default="damage_report.csv")
    ap.add_argument("--threshold", type=int, default=THRESHOLD)
    args = ap.parse_args()

    #Import model dependencies after parsing arguments.
    try:
        import segmentation_models_pytorch as smp
    except ImportError:
        sys.exit("Missing library. Run:  pip3 install torch torchvision segmentation-models-pytorch")
    try:
        from sipm_dataset import SiPMDataset
    except ImportError:
        sys.exit("sipm_dataset.py must be in this folder (next to run_inference.py).")

    if not os.path.isfile(args.model):
        sys.exit(f"Model file not found: {args.model}")
    if not os.path.isdir(args.cropped):
        sys.exit(f"No '{args.cropped}' folder found. Run from the project folder.")


    # makes sure that this gpu can be used if available, otherwise it will use the cpu
    device = "cuda" if torch.cuda.is_available() else \
             ("mps" if torch.backends.mps.is_available() else "cpu")
    print(f"device: {device}")

    # labels the model as a Unet with a resnet34 encoder, and loads the weights from the model file. It also sets the model to evaluation mode.
    model = smp.Unet("resnet34", encoder_weights=None, classes=3, in_channels=3)
    model.load_state_dict(torch.load(args.model, map_location=device))
    model.to(device).eval()
    print(f"loaded model: {args.model}")

    # This function takes in a path to an image, opens it, converts it to RGB, pads it to be square, resizes it to the specified size, and normalizes the pixel values. It returns a tensor that can be used as input to the model.
    def prep(path):
        img = SiPMDataset._pad_square(Image.open(path).convert("RGB"), 0).resize((SIZE, SIZE), Image.BILINEAR)
        return TF.normalize(TF.to_tensor(img), [0.485, 0.456, 0.406], [0.229, 0.224, 0.225])

    trays = sorted(d for d in os.listdir(args.cropped)
                   if os.path.isdir(os.path.join(args.cropped, d)))
    print(f"trays to process: {len(trays)}")

    #
    rows = []

    # runs all the chips through the model and saves the results to a csv file. It also prints a summary of the results for each tray.
    for ti, tray in enumerate(trays):
        td = os.path.join(args.cropped, tray)
        files = sorted(f for f in os.listdir(td) if f.lower().endswith(".png"))
        for fn in files:
            x = prep(os.path.join(td, fn)).unsqueeze(0).to(device)
            with torch.no_grad():
                pred = model(x).argmax(1)[0].cpu().numpy()
            dmg = int((pred == 1).sum())
            art = int((pred == 2).sum())
            cov = dmg + art
            rows.append(dict(tray=tray, chip=fn,
                             damage_px=dmg, artifact_px=art, coverage_px=cov,
                             damage_frac=round(dmg / (SIZE * SIZE), 6),
                             coverage_frac=round(cov / (SIZE * SIZE), 6),
                             predicted_damaged=int(dmg > args.threshold)))
        print(f"  [{ti+1}/{len(trays)}] {tray}: {len(files)} chips")

    fields = ["tray", "chip", "damage_px", "artifact_px", "coverage_px",
              "damage_frac", "coverage_frac", "predicted_damaged"]
    with open(args.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    print(f"\nwrote {args.out}  ({len(rows)} chips total)")

    # Print a per-tray summary. Less important but good for debugging/sanity  checks.
    from collections import defaultdict
    by = defaultdict(list)
    for r in rows:
        by[r["tray"]].append(r)
    print(f"\n{'tray':18} {'chips':>6} {'mean_dmg_frac':>14} {'%pred_damaged':>14}")
    for tray, rs in by.items():
        md = sum(r["damage_frac"] for r in rs) / len(rs)
        pd = 100 * sum(r["predicted_damaged"] for r in rs) / len(rs)
        print(f"{tray:18} {len(rs):>6} {md:>14.5f} {pd:>13.1f}%")


if __name__ == "__main__":
    main()

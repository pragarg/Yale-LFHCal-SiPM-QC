#!/usr/bin/env python3
"""Create prediction overlays for selected SiPM crops.

Red marks predicted damage and blue marks predicted artifact. Outputs are saved
under spot_check/<tray>/ by default.
"""
import argparse, csv, os, sys
import numpy as np
import torch
from PIL import Image
import torchvision.transforms.functional as TF

SIZE = 512


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="sipm_unet_final_3tray.pt")
    ap.add_argument("--cropped", default="cropped")
    ap.add_argument("--tray", required=True)
    ap.add_argument("--n", type=int, default=16, help="how many chips (default 16)")
    ap.add_argument("--chips", nargs="*", help="specific chip filenames")
    ap.add_argument("--top-coverage", action="store_true",
                    help="pick the highest-coverage chips (needs --report)")
    ap.add_argument("--top-damage", action="store_true",
                    help="pick the highest-damage chips (needs --report)")
    ap.add_argument("--report", default="damage_report.csv")
    ap.add_argument("--outdir", default="spot_check")
    args = ap.parse_args()

    try:
        import segmentation_models_pytorch as smp
        from sipm_dataset import SiPMDataset
    except ImportError as e:
        sys.exit(f"Missing dependency: {e}. Run inference setup first.")

    td = os.path.join(args.cropped, args.tray)
    if not os.path.isdir(td):
        sys.exit(f"No such tray folder: {td}")

    #select chips to process.
    if args.chips:
        chips = args.chips
    elif (args.top_coverage or args.top_damage) and os.path.isfile(args.report):
        key = "coverage_frac" if args.top_coverage else "damage_frac"
        rows = [r for r in csv.DictReader(open(args.report)) if r["tray"] == args.tray]
        rows.sort(key=lambda r: float(r[key]), reverse=True)
        chips = [r["chip"] for r in rows[:args.n]]
        print(f"top {len(chips)} chips by {key} in {args.tray}")
    else:
        chips = sorted(f for f in os.listdir(td) if f.lower().endswith(".png"))[:args.n]

    device = "cuda" if torch.cuda.is_available() else \
             ("mps" if torch.backends.mps.is_available() else "cpu")
    model = smp.Unet("resnet34", encoder_weights=None, classes=3, in_channels=3)
    model.load_state_dict(torch.load(args.model, map_location=device))
    model.to(device).eval()

    outdir = os.path.join(args.outdir, args.tray)
    os.makedirs(outdir, exist_ok=True)

    def prep(path):
        img = SiPMDataset._pad_square(Image.open(path).convert("RGB"), 0).resize((SIZE, SIZE), Image.BILINEAR)
        return img, TF.normalize(TF.to_tensor(img), [0.485, 0.456, 0.406], [0.229, 0.224, 0.225])

    for fn in chips:
        p = os.path.join(td, fn)
        if not os.path.isfile(p):
            print("  missing:", fn); continue
        img, x = prep(p)
        with torch.no_grad():
            pred = model(x.unsqueeze(0).to(device)).argmax(1)[0].cpu().numpy()
        ov = np.array(img).astype(float)
        ov[pred == 1] = 0.5 * ov[pred == 1] + 0.5 * np.array([255, 0, 0])
        ov[pred == 2] = 0.5 * ov[pred == 2] + 0.5 * np.array([0, 0, 255])
        dmg = int((pred == 1).sum()); art = int((pred == 2).sum())
        Image.fromarray(ov.astype("uint8")).save(os.path.join(outdir, fn))
        print(f"  {fn:22} damage_px={dmg:6d}  artifact_px={art:6d}")

    print(f"\nsaved {len(chips)} overlays to {outdir}/")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Re-run inference for one tray and update damage_report.csv.

Rows for the requested tray are replaced. Rows from other trays are preserved.
"""
import argparse, csv, os, sys, tempfile
import numpy as np, torch
from PIL import Image
import torchvision.transforms.functional as TF

SIZE = 512
THRESHOLD = 300
FIELDS = ["tray","chip","damage_px","artifact_px","coverage_px",
          "damage_frac","coverage_frac","predicted_damaged"]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tray", required=True)
    ap.add_argument("--model", default="sipm_unet_final_3tray.pt")
    ap.add_argument("--cropped", default="cropped")
    ap.add_argument("--csv", default="damage_report.csv")
    ap.add_argument("--threshold", type=int, default=THRESHOLD)
    args = ap.parse_args()

    import segmentation_models_pytorch as smp
    from sipm_dataset import SiPMDataset

    td = os.path.join(args.cropped, args.tray)
    if not os.path.isdir(td): sys.exit(f"No tray folder: {td}")

    device = "cuda" if torch.cuda.is_available() else \
             ("mps" if torch.backends.mps.is_available() else "cpu")
    model = smp.Unet("resnet34", encoder_weights=None, classes=3, in_channels=3)
    model.load_state_dict(torch.load(args.model, map_location=device))
    model.to(device).eval()
    print(f"model on {device}; re-running tray {args.tray}")

    def prep(p):
        img = SiPMDataset._pad_square(Image.open(p).convert("RGB"),0).resize((SIZE,SIZE),Image.BILINEAR)
        return TF.normalize(TF.to_tensor(img),[0.485,0.456,0.406],[0.229,0.224,0.225])

    new_rows=[]
    files=sorted(f for f in os.listdir(td) if f.lower().endswith(".png"))
    for fn in files:
        x=prep(os.path.join(td,fn)).unsqueeze(0).to(device)
        with torch.no_grad(): pred=model(x).argmax(1)[0].cpu().numpy()
        dmg=int((pred==1).sum()); art=int((pred==2).sum()); cov=dmg+art
        new_rows.append(dict(tray=args.tray, chip=fn, damage_px=dmg, artifact_px=art,
                             coverage_px=cov, damage_frac=round(dmg/(SIZE*SIZE),6),
                             coverage_frac=round(cov/(SIZE*SIZE),6),
                             predicted_damaged=int(dmg>args.threshold)))
    print(f"  {len(new_rows)} chips scored")

    #Replace this tray while keeping rows from other trays.
    kept=[]
    if os.path.isfile(args.csv):
        for r in csv.DictReader(open(args.csv)):
            if r["tray"]!=args.tray: kept.append(r)
        print(f"  kept {len(kept)} rows from other trays; replacing {args.tray}")
    else:
        print("  no existing CSV; writing fresh")

    #write to a temp file next to the real one and rename over it, so a crash partway through can't leave you with a half-written report
    outdir=os.path.dirname(os.path.abspath(args.csv))
    fd,tmp=tempfile.mkstemp(dir=outdir,suffix=".csv")
    try:
        with os.fdopen(fd,"w",newline="") as f:
            w=csv.DictWriter(f,fieldnames=FIELDS); w.writeheader()
            w.writerows(kept); w.writerows(new_rows)
        os.replace(tmp,args.csv)
    except BaseException:
        if os.path.exists(tmp): os.remove(tmp)
        raise
    print(f"updated {args.csv} ({len(kept)+len(new_rows)} rows total)")

if __name__=="__main__": main()

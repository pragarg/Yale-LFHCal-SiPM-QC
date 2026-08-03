#!/usr/bin/env python3

#straightens the photo of the SiPM, makes a gray scale image and makes a 
# mask of the SiPM so it can be cropped. After the crop, the area is padded
# symmetrical to ensure the convolutional neural network doesn't have a bias towards the corners. 
import argparse, os, sys, glob
import cv2
import numpy as np

VALID = (".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff")


def find_chip(bgr, thresh, min_area_frac):
    """Return a rotated rect ((cx,cy),(w,h),angle) for the bright chip, or None."""
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
    #threshold the bright chip against the darker holder.
    if thresh <= 0:
        _, mask = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    else:
        _, mask = cv2.threshold(gray, thresh, 255, cv2.THRESH_BINARY)

    #close small gaps and remove specks.
    k = max(3, int(min(bgr.shape[:2]) * 0.01))
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k))
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)

    cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not cnts:
        return None, mask

    img_area = bgr.shape[0] * bgr.shape[1]
    # Prefer the largest reasonably square blob near the center.
    h, w = bgr.shape[:2]
    cx0, cy0 = w / 2, h / 2
    best, best_score = None, -1
    for c in cnts:
        area = cv2.contourArea(c)
        if area < img_area * min_area_frac:
            continue
        rect = cv2.minAreaRect(c)
        (rcx, rcy), (rw, rh), ang = rect
        if rw < 5 or rh < 5:
            continue
        squareness = min(rw, rh) / max(rw, rh)
        centerness = 1 - (np.hypot(rcx - cx0, rcy - cy0) / np.hypot(cx0, cy0))
        fill = area / (rw * rh)
        score = area * (0.5 + 0.5 * squareness) * (0.6 + 0.4 * max(0, centerness)) * fill
        if score > best_score:
            best_score, best = score, rect
    return best, mask


def find_inner(gray, chip_rect, inner_thresh):
    """Find the darker active area inside the bright chip frame."""
    H, W = gray.shape
    box = cv2.boxPoints(chip_rect).astype(np.int32)
    chip_mask = np.zeros((H, W), np.uint8)
    cv2.fillPoly(chip_mask, [box], 255)
    interior = gray[chip_mask > 0]
    if interior.size == 0:
        return None
    if inner_thresh <= 0:
        # Split the gray center from the bright frame.
        t, _ = cv2.threshold(interior, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    else:
        t = inner_thresh
    _, frame = cv2.threshold(gray, t, 255, cv2.THRESH_BINARY)
    frame = cv2.bitwise_and(frame, chip_mask)
    k = max(3, int(min(H, W) * 0.015))
    kern = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k))
    # Seal frame gaps before flood-fill.
    frame = cv2.morphologyEx(frame, cv2.MORPH_CLOSE, kern, iterations=4)
    ff = frame.copy()
    m = np.zeros((H + 2, W + 2), np.uint8)
    cv2.floodFill(ff, m, (0, 0), 255)
    holes = cv2.bitwise_and(cv2.bitwise_not(ff), chip_mask)
    holes = cv2.morphologyEx(holes, cv2.MORPH_OPEN, kern, iterations=2)
    cnts, _ = cv2.findContours(holes, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    chip_area = chip_rect[1][0] * chip_rect[1][1]
    #Keep plausible active-area candidates.
    cand = [c for c in cnts if 0.12 * chip_area < cv2.contourArea(c) < 0.85 * chip_area]
    if not cand:
        return None
    return cv2.minAreaRect(max(cand, key=cv2.contourArea))


def crop_rotated(bgr, rect, margin):
    """De-rotate and crop the rotated rect, with a fractional margin around it."""
    (cx, cy), (rw, rh), ang = rect
    rw, rh = rw * (1 + margin), rh * (1 + margin)
    # Normalize the rotated rectangle orientation.
    if ang < -45:
        ang += 90
        rw, rh = rh, rw
    M = cv2.getRotationMatrix2D((cx, cy), ang, 1.0)
    h, w = bgr.shape[:2]
    #Preserve original pixel values during rotation.
    rotated = cv2.warpAffine(bgr, M, (w, h), flags=cv2.INTER_NEAREST,
                             borderMode=cv2.BORDER_REPLICATE)
    out = cv2.getRectSubPix(rotated, (int(round(rw)), int(round(rh))), (cx, cy))
    return out


def crop_active_padded(bgr, chip_rect, inner_thresh, pad_px):
    """Crop the inner active area with symmetric padding."""
    H, W = bgr.shape[:2]
    (cx, cy), (cw, ch), ang = chip_rect # center, size, angle of the chip
    if ang < -45:
        ang += 90
    M = cv2.getRotationMatrix2D((cx, cy), ang, 1.0)
    rot = cv2.warpAffine(bgr, M, (W, H), flags=cv2.INTER_NEAREST,
                         borderMode=cv2.BORDER_REPLICATE)
    gray = cv2.cvtColor(rot, cv2.COLOR_BGR2GRAY) #turns into grayscale since color isn't needed
    x0, y0 = max(0, int(cx - cw / 2)), max(0, int(cy - ch / 2))
    x1, y1 = min(W, int(cx + cw / 2)), min(H, int(cy + ch / 2))
    roi = gray[y0:y1, x0:x1] #finds the region of interest (roi) which is the area of the chip 
    if roi.size == 0:
        return None
    chipmask = np.zeros_like(gray) #creates a stencil mask of the chip area to ignore the background
    chipmask[y0:y1, x0:x1] = 255
    k = max(3, int(min(H, W) * 0.015))
    kern = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k))
    chip_area = cw * ch

    #now with everything in grayscale and the mask/stencil created, we can find the inner active area by thresholding the image
    def holes_for(t):
        _, frame = cv2.threshold(gray, t, 255, cv2.THRESH_BINARY)
        frame = cv2.bitwise_and(frame, chipmask)
        frame = cv2.morphologyEx(frame, cv2.MORPH_CLOSE, kern, iterations=4)
        ff = frame.copy()
        m = np.zeros((H + 2, W + 2), np.uint8)
        cv2.floodFill(ff, m, (0, 0), 255)
        holes = cv2.bitwise_and(cv2.bitwise_not(ff), chipmask)
        holes = cv2.morphologyEx(holes, cv2.MORPH_OPEN, kern, iterations=2)
        cnts, _ = cv2.findContours(holes, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        return [c for c in cnts if 0.12 * chip_area < cv2.contourArea(c) < 0.85 * chip_area]

    t = inner_thresh if inner_thresh > 0 else \
        int(cv2.threshold(roi, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)[0])
    cand = holes_for(t)
    if not cand and inner_thresh <= 0:
        #retry with a higher threshold if the first automatic split fails.
        bright = roi[roi > t]
        if bright.size > 50:
            t2 = int(cv2.threshold(bright, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)[0])
            if t2 > t:
                cand = holes_for(t2)
    if not cand:
        return None
    
    #cuts out the portion that isn't used in the crop, and then fills the area with padding for the convolutional neural network to use. The padding is symmetric so that the corners aren't different.
    bx, by, bw, bh = cv2.boundingRect(max(cand, key=cv2.contourArea))
    bx, by = bx - pad_px, by - pad_px
    bw, bh = bw + 2 * pad_px, bh + 2 * pad_px
    bx, by = max(0, bx), max(0, by)
    bw, bh = min(W - bx, bw), min(H - by, bh)
    crop = rot[by:by + bh, bx:bx + bw]
    if crop.size == 0:
        return None
    # Map crop corners back to the original image for the debug overlay.
    Minv = cv2.invertAffineTransform(M)
    corners = np.array([[bx, by], [bx + bw, by], [bx + bw, by + bh], [bx, by + bh]],
                       np.float32).reshape(-1, 1, 2)
    corners = cv2.transform(corners, Minv).reshape(-1, 2)
    return crop, corners


def draw_debug(bgr, chip_rect, crop_rect):
    """Draw the detected chip and crop region."""
    dbg = bgr.copy()
    lw = max(2, bgr.shape[1] // 400)
    cv2.drawContours(dbg, [cv2.boxPoints(chip_rect).astype(int)], 0, (0, 255, 255), lw)
    cv2.drawContours(dbg, [cv2.boxPoints(crop_rect).astype(int)], 0, (255, 80, 255), lw)
    return dbg


def list_images(folder):
    return [p for p in sorted(glob.glob(os.path.join(folder, "*")))
            if p.lower().endswith(VALID)]


def process_folder(inp, out, debug, args):
    """Crop every image in one folder. Returns (ok, total, failed, fellback)."""
    os.makedirs(out, exist_ok=True)
    if debug:
        os.makedirs(debug, exist_ok=True)
    paths = list_images(inp)
    ok, failed, fellback = 0, [], 0
    for p in paths:
        name = os.path.basename(p)
        bgr = cv2.imread(p)
        if bgr is None:
            failed.append(name + "  (unreadable)")
            continue
        chip_rect, _ = find_chip(bgr, args.thresh, args.min_area)
        if chip_rect is None:
            failed.append(name + "  (no chip found)")
            continue

        # Inner mode: straight bounding box around the active area.
        if args.inner:
            res = crop_active_padded(bgr, chip_rect, args.inner_thresh, args.pad)
            if res is not None:
                crop, corners = res
                if args.size > 0:
                    crop = cv2.resize(crop, (args.size, args.size),
                                      interpolation=cv2.INTER_AREA)
                    # Map the resized corners for the debug overlay.
                cv2.imwrite(os.path.join(out, name), crop)
                if debug:
                    dbg = bgr.copy()
                    lw = max(2, bgr.shape[1] // 400)
                    cv2.drawContours(dbg, [cv2.boxPoints(chip_rect).astype(int)],
                                     0, (0, 255, 255), lw)
                    cv2.polylines(dbg, [corners.astype(int)], True, (255, 80, 255), lw)
                    cv2.imwrite(os.path.join(debug, name), dbg)
                ok += 1
                continue
            fellback += 1

        # Full-chip crop, used for outer mode or inner fallback.
        margin = args.margin if args.margin is not None else 0.06
        crop = crop_rotated(bgr, chip_rect, margin)
        if crop is None or crop.size == 0:
            failed.append(name + "  (empty crop)")
            continue
        if args.size > 0:
            crop = cv2.resize(crop, (args.size, args.size), interpolation=cv2.INTER_AREA)
        cv2.imwrite(os.path.join(out, name), crop)
        if debug:
            (cx, cy), (rw, rh), ang = chip_rect
            crop_rect = ((cx, cy), (rw * (1 + margin), rh * (1 + margin)), ang)
            cv2.imwrite(os.path.join(debug, name), draw_debug(bgr, chip_rect, crop_rect))
        ok += 1
    return ok, len(paths), failed, fellback

 
def main():
    ap = argparse.ArgumentParser(description="Crop + de-rotate SiPM active areas.")
    ap.add_argument("--in", dest="inp", required=True,
                    help="folder of images, OR a parent folder of tray subfolders")
    ap.add_argument("--out", required=True, help="folder for cropped output")
    ap.add_argument("--debug", default=None, help="folder for detection overlays")
    ap.add_argument("--thresh", type=int, default=0,
                    help="brightness threshold 0-255; 0 = automatic (Otsu)")
    ap.add_argument("--inner", action="store_true",
                    help="crop to the inner ACTIVE square (the gray microcell area) "
                         "instead of the whole chip; use this if you only grade the center")
    ap.add_argument("--pad", type=int, default=20,
                    help="with --inner: pixels added equally on all four sides around "
                         "the active area (default 20). Symmetric, so no corner bias")
    ap.add_argument("--inner-thresh", type=int, default=0,
                    help="brightness split between active center and bright frame; "
                         "0 = automatic (Otsu on the chip interior)")
    ap.add_argument("--margin", type=float, default=None,
                    help="fractional margin around the crop. Default: 0.06 for the "
                         "full chip, 0.05 for --inner (keeps the dark outline + a thin "
                         "sliver of white frame so you can see nothing was clipped). "
                         "Raise it for more border, lower it for a tighter crop")
    ap.add_argument("--min-area", type=float, default=0.02,
                    help="ignore blobs smaller than this fraction of the image")
    ap.add_argument("--size", type=int, default=0,
                    help="if >0, resize every crop to size x size (square)")
    args = ap.parse_args()

    if not os.path.isdir(args.inp):
        sys.exit(f"Not a folder: {args.inp}")

    # Use single-folder mode if images are directly in --in; otherwise process tray subfolders.
    direct = list_images(args.inp)
    subdirs = [d for d in sorted(os.listdir(args.inp))
               if os.path.isdir(os.path.join(args.inp, d)) and list_images(os.path.join(args.inp, d))]

    if direct:
        trays = [(args.inp, args.out, args.debug)]
        print(f"Single folder mode: {len(direct)} images.")
    elif subdirs:
        trays = [(os.path.join(args.inp, d), os.path.join(args.out, d),
                  os.path.join(args.debug, d) if args.debug else None) for d in subdirs]
        print(f"Batch mode: {len(subdirs)} tray folders -> {subdirs}")
    else:
        sys.exit(f"No images found in {args.inp} (or its subfolders).")

    grand_ok = grand_total = grand_fell = 0
    all_failed = []
    for inp, out, debug in trays:
        tray = os.path.basename(inp.rstrip("/\\"))
        ok, total, failed, fell = process_folder(inp, out, debug, args)
        grand_ok += ok; grand_total += total; grand_fell += fell
        all_failed += [f"[{tray}] {f}" for f in failed]
        if len(trays) > 1:
            print(f"  {tray}: {ok}/{total} cropped -> {out}")

    print(f"\nDone. {grand_ok}/{grand_total} cropped.")
    if args.inner and grand_fell:
        print(f"{grand_fell} had no detectable inner square and fell back to the full "
              f"chip — check those overlays and tune --inner-thresh if needed.")
    if args.debug:
        print("Detection overlays saved.")
    if all_failed:
        print(f"\n{len(all_failed)} need attention:")
        for f in all_failed:
            print("  -", f)
        print("\nTip: if many failed, try a different --thresh (e.g. 60-90), "
              "lower --min-area, or tune --inner-thresh.")


if __name__ == "__main__":
    main()

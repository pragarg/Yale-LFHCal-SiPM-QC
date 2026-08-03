# Command sheet

Every command below is run from your **data folder** — the one holding `raw/` —
not from inside this project folder. `$SIPM` below is wherever this project
lives, e.g.:

```
export SIPM=~/repos/big-repo/sipm-photo-qa
```

Your data folder should look like this. None of it is in git; it's yours.

```
my-sipm-data/
  raw/                 one subfolder per tray of raw microscope photos
    260409-1702/
    250812-1301/
  cropped/             created by step 1
  debug/               created by step 1, crop overlays to eyeball
  *.json               created by step 2, your label exports
```

---

## 0. One-time setup

```
pip3 install -r $SIPM/requirements.txt
chmod +x $SIPM/scripts/crop.sh
```

If you only want to crop and read stats, you can skip torch — numpy, pillow and
opencv are enough. torch is only needed once you start scoring chips.

---

## 1. Crop (raw -> cropped)

One tray:

```
$SIPM/scripts/crop.sh 260312-1901
```

Several:

```
$SIPM/scripts/crop.sh 260312-1901 250812-1301 250812-1302
```

Long form, if you want to change flags:

```
python3 $SIPM/scripts/prep_sipm_crops.py \
    --in raw/260312-1901 --out cropped/260312-1901 \
    --debug debug/260312-1901 --inner
```

Then look through `debug/<tray>/` and confirm the magenta box landed on the
active area. **Never re-crop a tray you have already labeled** — the masks are
stored in crop pixel coordinates and would no longer line up.

Flags worth knowing:

| flag | what it does |
|---|---|
| `--inner` | crop to the active square, not the whole chip. You basically always want this |
| `--pad 20` | border kept around the active area, in px |
| `--inner-thresh 150` | force the centre-vs-frame brightness split for one stubborn tray |

Check the crops came out sane:

```
python3 $SIPM/scripts/count_photos.py --dir cropped
python3 $SIPM/scripts/check_crops.py
```

`count_photos` expects 460 per tray (20 rows x 23 cols) and says so if a tray is
short. `check_crops` writes `bad_crops.csv` listing anything lopsided or the
wrong size.

---

## 2. Label

Open `$SIPM/labeling-tool/sipm_labeler.html` in a browser. No install, nothing
runs on a server, the file is self-contained.

- **Load folder** -> pick `cropped/<tray>`
- Paint **red** for real damage, **blue** for false artifacts (debris, hair, glare)
- Tick the types, set verdict and grade
- **Ctrl+S** exports the labels JSON — do this often, not just at the end
- **Resume**: load the IMAGES first, *then* Resume and pick the JSON

Keys: `0` pristine+next · `1`–`5` grade · `B`/`E` brush/erase · `D`/`A`
damage/false · `N` next unlabeled · `[` `]` brush size

Those blue artifact masks on otherwise-pristine chips are the most valuable
thing you can paint. They're what stops the model calling every speck of dust a
scratch.

---

## 3. Stats

```
python3 $SIPM/scripts/label_stats.py *.json
```

Prints pristine/damaged split, per-grade counts and painted-area distributions,
suggested grade cutoffs, and a discrepancy audit. Writes `per_image_stats.csv`.

The discrepancy audit is the part to actually read — it catches chips marked
damaged with nothing painted, or pristine with a red mask. Those are labeling
slips and they poison training.

---

## 4. Train (Colab, not your terminal)

Cropping is local; training needs a GPU.

In Google Drive make a folder `sipm` containing your label `.json` files,
`cropped/` with the tray subfolders inside, and a copy of
`$SIPM/scripts/sipm_dataset.py`.

Then at colab.research.google.com:

1. Runtime -> Change runtime type -> **GPU** -> Save
2. Upload and run `$SIPM/notebooks/SiPM_Step1_Colab_3.ipynb` (confirms data loads)
3. Then `$SIPM/notebooks/SiPM_Step2_Train_Colab_2.ipynb` (trains, saves the .pt)

Run cells top to bottom. Cell 1 installs the libraries — let it finish before
moving on.

The last cell saves a checkpoint into your Drive `sipm` folder. The filename
depends on MODE:

```
sipm_unet_<tag>_val-250812-1302.pt     MODE='holdout'
sipm_unet_<tag>_final_alltrays.pt      MODE='final'
```

Download that file and point `--model` at it to score chips with your own model
instead of the one shipped in `model/`:

```
python3 $SIPM/scripts/run_inference.py --model ~/Downloads/sipm_unet_..._final_alltrays.pt
```

Step 1 just confirms your data loads — worth running before you burn GPU time.
Step 2 does the training.

Step 2 has a switch at the top:

```
MODE     = 'holdout'          # honest eval: holds out a whole tray
VAL_TRAY = '250812-1302'      # rotate this to cross-validate
```

Use `holdout` for numbers you'd actually quote, rotating `VAL_TRAY` through your
trays. Switch to `MODE='final'` only for the last run that produces the shipped
checkpoint — its val numbers are meaningless because nothing is held out.

---

## 4.5 Try it without any data of your own

Six real cropped chips ship in `sample-data/` — three the model flags, three it
doesn't. Enough to confirm your install works:

```
cd $SIPM/sample-data
python3 ../scripts/run_inference.py --model ../model/sipm_unet_final_3tray.pt
```

Writes `damage_report.csv` in `sample-data/`. Expect 3 of 6 with
`predicted_damaged=1`. Then look at what it drew:

```
python3 ../scripts/spot_check.py --tray 260409-1702 --n 6 \
    --model ../model/sipm_unet_final_3tray.pt
```

---

## 5. Score every chip

```
python3 $SIPM/scripts/run_inference.py --model $SIPM/model/sipm_unet_final_3tray.pt
```

Writes `damage_report.csv`, one row per chip. On CPU a full tray takes a while;
use a GPU machine if you have one.

Re-do a single tray after re-cropping or re-labeling it, leaving the other
trays' rows alone:

```
python3 $SIPM/scripts/update_tray.py --tray 250812-1301 \
    --model $SIPM/model/sipm_unet_final_3tray.pt
```

Look at what the model is drawing:

```
python3 $SIPM/scripts/spot_check.py --tray 250812-1301 --top-damage --n 20 \
    --model $SIPM/model/sipm_unet_final_3tray.pt
```

Overlays land in `spot_check/<tray>/`. Red is predicted damage, blue is
predicted artifact.

---

## 6. Correlate damage against SPS breakdown voltage

```
python3 $SIPM/scripts/sipm_sps_damage_analysis.py \
    --damage-csv damage_report.csv \
    --sps-root robot_production --sps-root cassette_production \
    --output-dir outputs/sipm_sps_damage_raw
```

Produces the merged CSV, a per-tray summary, a join audit, and an interactive
HTML scatter.

Read `sps_join_audit.csv` before believing the scatter. It tells you which trays
joined by name, which joined through `configs/sps_damage_tray_aliases.csv`, and
how many chips got dropped. The alias file asserts that two differently-named
trays are the same physical tray, and nothing can verify that for you — if a
result leans on aliased trays, re-run with an empty alias file and check it
survives.

---

## When something breaks

| Problem | Fix |
|---|---|
| `raw/<id> not found` | you're not in your data folder |
| `No 'cropped' folder here` | same — cd to the data folder |
| crop grabbed the whole chip, not the centre | usually rescued automatically; else add `--inner-thresh 150` for that tray |
| Resume restores nothing | load the tray's images first, *then* Resume |
| same filename in two trays | expected — every tray has `SiPM_R00_C00`. The tray folder keeps them apart |
| `No module named segmentation_models_pytorch` | `pip3 install -r requirements.txt`; in Colab, re-run cell 1 then Runtime -> Restart session |
| Colab says no GPU | Runtime -> Change runtime type -> GPU |

---

## The loop for each new tray

```
$SIPM/scripts/crop.sh <tray-id>              # 1. crop
                                             # 2. label in the browser, Ctrl+S
python3 $SIPM/scripts/label_stats.py *.json  # 3. check stats + audit
                                             # 4. new JSON + cropped/<tray> into Drive, retrain
python3 $SIPM/scripts/update_tray.py --tray <tray-id> --model $SIPM/model/sipm_unet_final_3tray.pt
```

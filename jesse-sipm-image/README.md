# **SiPM photo QA**

Finds surface damage in microscope photos of SiPMs. A U-Net segments each chip into background, damage, or artifact, and chips with enough predicted damage get flagged for a human to look at. Refer to COMMANDS.md for an in-depth command sheet and guide.

## **Install**

Needs Python 3.10 or newer.

```
pip3 install -r requirements.txt
```

If you only want to crop photos and read label stats, numpy \+ pillow \+ opencv-python are enough; you can skip torch. torch is only needed once you start scoring chips with the model.

## **Program verification**

Six real cropped chips ship in `sample-data/`, so you can test the install without any photos of your own.

```
cd sample-data
python3 ../scripts/run_inference.py --model ../model/sipm_unet_final_3tray.pt
```

You should get `damage_report.csv` with 6 rows, 3 of them `predicted_damaged=1`, and a printed `%pred_damaged` of 50.0. To see what the model actually came up with:

```
python3 ../scripts/spot_check.py --tray 260409-1702 --n 6 \
    --model ../model/sipm_unet_final_3tray.pt
```

Overlays land in `sample-data/spot_check/260409-1702/`. Red is predicted damage, blue is predicted artifact.

## **What's in here**

```
scripts/         the pipeline — cropping, stats, inference, analysis
notebooks/       Colab notebooks for training (step 1 checks data, step 2 trains)
labeling-tool/   sipm_labeler.html, open it in a browser, no install
model/           the trained checkpoint
sample-data/     six chips so you can test the install
configs/         tray name aliases used by the SPS analysis
examples/        sample overlays and plots
```

# **Building the dataset**

The photos are not in the repo; rather, you will make a folder with your own photos. Here is how to do it:

```
my-sipm-data/
  raw/
    260409-1702/       one subfolder per tray of raw photos
```

Every command below runs from `my-sipm-data/`, not from inside this repo, with `$SIPM` pointing at the repo.

## **1\. Crop**

Raw photos show the whole chip sitting in its holder. The model only looks at the active area which is the gray microcell square inside the white frame. That will get cut out first.

```
$SIPM/scripts/crop.sh 260409-1702
```

Several trays at once:

```
$SIPM/scripts/crop.sh 260409-1702 250812-1301 250812-1302
```

This reads `raw/<tray>`, writes `cropped/<tray>`, and puts detection overlays in `debug/<tray>`.

**Look at the debug overlays before you do anything else.** Yellow is the chip the script found, magenta is what it actually cut out. If the magenta box is wrapped around the whole chip instead of the inner square, that tray needs a different threshold. This is why the debugging scripts will be your best friend\!

```
python3 $SIPM/scripts/prep_sipm_crops.py \
    --in raw/260409-1702 --out cropped/260409-1702 \
    --debug debug/260409-1702 --inner --inner-thresh 150
```

Then check the whole batch came out good:

```
python3 $SIPM/scripts/count_photos.py --dir cropped
python3 $SIPM/scripts/check_crops.py
```

`count_photos` expects 460 per tray (20 rows x 23 columns) and says so if one is short. `check_crops` writes `bad_crops.csv` listing anything lopsided or far off the tray's median size.

**Don't re-crop a tray you've already labeled.** The masks are stored in crop pixel coordinates and won't line up anymore.

## **2\. Label**

Open `$SIPM/labeling-tool/sipm_labeler.html` in a browser. You dont need to install anything since its a html file; it just opens up\!

Drag the whole `cropped/<tray-id>` folder into the window, or use **Load folder**.

For each chip:

* **Verdict** —\> pristine or damaged  
* **Paint red** over real damage. This counts toward the grade  
* **Paint blue** over hairs, dust, and glare, so the model learns to ignore them  
* **Damage type** —\> scratch, epoxy bubble/void, or false (debris/hair/glare)  
* **Severity grade** —\> 0 is pristine, 5 is worst (\~500–800 affected microcells out of 7,200)

Most chips are clean, so you can press 0 which auto classifies the chip as pristine and moves to the next photo.

|  |  |
| ----- | ----- |
| Prev / next image | `←` `→` |
| Next unlabeled | `N` |
| Mark pristine \+ advance | `0` |
| Set grade 1–5 | `1` … `5` |
| Brush / eraser | `B` / `E` |
| Damage / false class | `D` / `A` |
| Brush size − / \+ | `[` `]` |
| Pan view | `V` or hold `Space` |
| Zoom | scroll wheel |
| Fit to view | `F` |
| Undo stroke | `Ctrl`/`⌘` \+ `Z` |
| Export labels | `Ctrl`/`⌘` \+ `S` |

`Ctrl+S` exports a JSON of your labels. **Try to export often instead of just at the end so you dont lose all of your progress\!**

To pick a tray back up later, load the images first, then click **Resume** and choose the JSON.

Don’t be lazy with the blue masks on the otherwise clean chips. They're what stops the model calling every speck of dust a scratch.

## **3\. Check your labels**

```
python3 $SIPM/scripts/label_stats.py *.json
```

Prints the pristine/damaged split, per-grade counts, painted-area distributions, and suggested grade cutoffs. Writes `per_image_stats.csv`.

The part actually worth reading is the discrepancy audit at the bottom. It flags chips marked damaged with nothing painted, or marked pristine with a red mask. Those are labeling slips, and they'll poison training if you leave them in.

# **Scoring chips**

```
python3 $SIPM/scripts/run_inference.py --model $SIPM/model/sipm_unet_final_3tray.pt
```

Writes `damage_report.csv`, one row per chip, with damage pixels, artifact pixels, and coverage fractions.

To redo a single tray after re-cropping or re-labeling it, leaving every other tray's rows alone:

```
python3 $SIPM/scripts/update_tray.py --tray 260409-1702 \
    --model $SIPM/model/sipm_unet_final_3tray.pt
```

## **Damage vs SPS breakdown voltage**

# This part is separate from the photo QA. It asks whether chips the model calls damaged have a different breakdown voltage than the rest of their tray. Each chip gets its own tray's mean Raw\_VBD subtracted off, so tray-to-tray offsets don't sway the within-tray SPS breakdown voltage signals.

# **This needs SPS data that isn't in the repo.** You supply it, laid out one folder per tray:

```
robot_production/
  260115-1002/
    debrecen/
      SPS_result_onlynumbers.txt     <- the file that actually gets read
    summary/
      SPS summary                    <- optional, used as a cross-check
```

# Each line of `SPS_result_onlynumbers.txt` is one chip:

```
260115-1002_0_0  4  400  37.2974  18.9186  0.00359894  37.5042  0.0119298  2.55629 ...
```

# which is `<tray>_<i>_<j>`, peaks used, fit width, **Raw\_VBD**, avg temp °C, Raw\_VBD error, VBD at 25 °C, its error, chi2/ndf. Raw\_VBD is the one the analysis uses.

# Watch the coordinates: SPS numbers chips `tray_i_j`, but the photo filenames are `R<j>_C<i>`. Row and column are swapped between the two systems. The script handles it, but don't assume they line up if you're checking by hand.

# Run it from your data folder, once `damage_report.csv` exists:

```
python3 $SIPM/scripts/sipm_sps_damage_analysis.py \
    --damage-csv damage_report.csv \
    --sps-root robot_production --sps-root cassette_production \
    --output-dir outputs/sipm_sps_damage_raw
```

# `--sps-root` can be repeated as many times as you have collections. You get back five files:

| file | what's in it |
| ----- | ----- |
| `merged_sps_damage_raw.csv` | one row per chip, damage numbers joined to voltages |
| `per_tray_summary.csv` | per-tray correlations and counts |
| `sps_join_audit.csv` | which trays joined, how, and what got dropped |
| `analysis_summary.txt` | the headline numbers |
| `damage_vs_raw_sps_interactive.html` | scatter plot you can open in a browser |

# Read `sps_join_audit.csv` before you trust the plot**.** It's able to catch any problems. On real data it has flagged trays whose SPS ID didn't match their folder name, a duplicate SPS file that got excluded, and trays that came in short of the expected 460 rows. None of those are visible in the scatter.

# There's another thing to note: `configs/sps_damage_tray_aliases.csv` claims that certain differently-named trays are the same physical tray. Nothing in the code can check that, and if a row is wrong you'd be correlating one tray's damage against another tray's voltages. If a result depends on aliased trays, re-run with an empty alias file and confirm it survives the test.

# Two flags to know about `--exclude-retests` skips `-retest` trays instead of folding them into the base tray, and `--include-duplicate-sps` keeps SPS files whose contents duplicate an earlier one instead of dropping them.

# 

# **Training your own model**

Training happens in Colab, not locally. See COMMANDS.md section 4\.


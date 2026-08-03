#!/bin/bash
# Crop one tray by ID. Usage:  ./crop.sh 260312-1901
# Crops raw/<ID> -> cropped/<ID>, with overlays in debug/<ID>. Only that tray.
# Add more IDs to do several:   ./crop.sh 260312-1901 250812-1301 250812-1302

HERE="$(cd "$(dirname "$0")" && pwd)"   #so this works from your data folder, wherever the repo lives

if [ $# -eq 0 ]; then
  echo "Usage: ./crop.sh <tray-id> [more-ids...]"
  echo "Example: ./crop.sh 260312-1901"
  exit 1
fi

for ID in "$@"; do
  if [ ! -d "raw/$ID" ]; then
    echo "!! raw/$ID not found — skipping. (Run this from the folder that contains 'raw'.)"
    continue
  fi
  echo "=== cropping tray $ID ==="
  python3 "$HERE/prep_sipm_crops.py" --in "raw/$ID" --out "cropped/$ID" --debug "debug/$ID" --inner
  echo ""
done
echo "All done. Check the debug/<id> overlays before labeling."

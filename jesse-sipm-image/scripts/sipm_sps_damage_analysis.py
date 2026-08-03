#!/usr/bin/env python3
"""Join the CNN damage numbers to the SPS breakdown-voltage measurements.

The question this answers: do chips the CNN calls damaged sit at a different
breakdown voltage than their tray-mates? Each chip gets its tray mean Raw_VBD
subtracted off, so tray-to-tray offsets don't wash out the within-tray signal.

Raw_VBD comes from <tray>/debrecen/SPS_result_onlynumbers.txt. Everything is
opened read-only; all output goes under --output-dir.

    python3 /path/to/scripts/sipm_sps_damage_analysis.py \
        --damage-csv damage_report.csv \
        --sps-root robot_production --sps-root cassette_production
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import html
import json
import math
import re
from collections import defaultdict
from pathlib import Path
from statistics import mean, pstdev


# Defaults assume you are standing in your data folder. The alias CSV is the
# one exception: it ships with the code, so it resolves next to this file.
DEFAULT_DAMAGE_CSV = Path("damage_report.csv")
DEFAULT_ROBOT_ROOT = Path("robot_production")
DEFAULT_CASSETTE_ROOT = Path("cassette_production")
DEFAULT_ALIAS_CSV = Path(__file__).resolve().parent.parent / "configs" / "sps_damage_tray_aliases.csv"
DEFAULT_OUTPUT_DIR = Path("outputs/sipm_sps_damage_raw")
ROWS = 20
COLS = 23
EXPECTED_PER_TRAY = ROWS * COLS


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Join raw SPS VBD data to CNN damage data and create plots."
    )
    parser.add_argument("--damage-csv", type=Path, default=DEFAULT_DAMAGE_CSV)
    parser.add_argument(
        "--alias-csv",
        type=Path,
        default=DEFAULT_ALIAS_CSV,
        help="Optional CSV with columns sps_tray,damage_tray for known tray-name corrections.",
    )
    parser.add_argument(
        "--sps-root",
        type=Path,
        action="append",
        help="Root containing tray/debrecen/SPS_result_onlynumbers.txt. Can be repeated.",
    )
    parser.add_argument(
        "--robot-root",
        type=Path,
        default=DEFAULT_ROBOT_ROOT,
        help="Legacy single SPS root. Ignored when --sps-root is supplied.",
    )
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument(
        "--exclude-retests",
        action="store_true",
        help="Skip SPS trays ending in -retest instead of joining to the base damage tray.",
    )
    parser.add_argument(
        "--include-duplicate-sps",
        action="store_true",
        help="Include SPS files whose raw contents duplicate an earlier SPS file.",
    )
    return parser.parse_args()


def default_sps_roots(robot_root: Path) -> list[Path]:
    # Trays live under one of two collections depending on when they were
    # measured. Pick up the second automatically if it happens to be there.
    roots = [robot_root]
    if DEFAULT_CASSETTE_ROOT.exists() and DEFAULT_CASSETTE_ROOT not in roots:
        roots.append(DEFAULT_CASSETTE_ROOT)
    return roots


def read_aliases(path: Path) -> dict[str, dict[str, str]]:
    """Map SPS tray names onto damage tray names where the two disagree.

    Treat this file with suspicion. Every row here asserts that two differently
    named trays are the same physical tray, and nothing in the code can check
    that. A wrong row silently correlates one tray's damage against another
    tray's voltages. If a result depends on aliased trays, re-run with an empty
    alias CSV and confirm it survives.
    """
    if not path.exists():
        return {}
    aliases: dict[str, dict[str, str]] = {}
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        required = {"sps_tray", "damage_tray"}
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"Alias CSV missing columns: {sorted(missing)}")
        for row in reader:
            sps_tray = row["sps_tray"].strip()
            damage_tray = row["damage_tray"].strip()
            if not sps_tray or not damage_tray:
                continue
            aliases[sps_tray] = {
                "damage_tray": damage_tray,
                "note": row.get("note", "").strip(),
            }
    return aliases


def fnum(value: str) -> float:
    return float(value.strip())


def read_damage(path: Path) -> tuple[dict[tuple[str, int, int], dict], set[str]]:
    rows: dict[tuple[str, int, int], dict] = {}
    trays: set[str] = set()
    chip_re = re.compile(r"R(\d+)_C(\d+)")
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        required = {
            "tray",
            "chip",
            "damage_px",
            "artifact_px",
            "coverage_px",
            "damage_frac",
            "coverage_frac",
            "predicted_damaged",
        }
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"Damage CSV missing columns: {sorted(missing)}")
        for row in reader:
            match = chip_re.search(row["chip"])
            if not match:
                raise ValueError(f"Could not parse chip coordinate: {row['chip']}")
            r = int(match.group(1))
            c = int(match.group(2))
            tray = row["tray"]
            trays.add(tray)
            key = (tray, r, c)
            if key in rows:
                raise ValueError(f"Duplicate damage row for {key}")
            rows[key] = {
                "damage_tray": tray,
                "chip": row["chip"],
                "row": r,
                "col": c,
                "damage_px": int(float(row["damage_px"])),
                "artifact_px": int(float(row["artifact_px"])),
                "coverage_px": int(float(row["coverage_px"])),
                "damage_frac": fnum(row["damage_frac"]),
                "coverage_frac": fnum(row["coverage_frac"]),
                "predicted_damaged": int(float(row["predicted_damaged"])),
            }
    return rows, trays


def parse_sps_id(sps_id: str) -> tuple[str, int, int]:
    match = re.match(r"(.+?)_(\d+)_(\d+)$", sps_id)
    if not match:
        raise ValueError(f"Could not parse SPS ID: {sps_id}")
    return match.group(1), int(match.group(2)), int(match.group(3))


def read_sps_numbers(path: Path) -> list[dict]:
    records: list[dict] = []
    with path.open() as handle:
        for line_no, line in enumerate(handle, start=1):
            stripped = line.strip()
            if not stripped:
                continue
            fields = stripped.split()
            if len(fields) < 9:
                raise ValueError(f"{path}:{line_no} has {len(fields)} fields, expected >= 9")
            sps_tray, sps_i, sps_j = parse_sps_id(fields[0])
            # SPS coordinate convention is tray_i_j, while CNN chip names are Rj_Ci.
            row = sps_j
            col = sps_i
            if not (0 <= row < ROWS and 0 <= col < COLS):
                raise ValueError(f"{path}:{line_no} coordinate out of range: {fields[0]}")
            records.append(
                {
                    "sps_id": fields[0],
                    "sps_tray": sps_tray,
                    "sps_i": sps_i,
                    "sps_j": sps_j,
                    "row": row,
                    "col": col,
                    "peaks_used": int(float(fields[1])),
                    "fit_width": int(float(fields[2])),
                    "raw_sps_field_count": len(fields),
                    "raw_vbd": fnum(fields[3]),
                    "avg_temp_c": fnum(fields[4]),
                    "raw_vbd_err": fnum(fields[5]),
                    "vbd_25c": fnum(fields[6]),
                    "vbd_25c_err": fnum(fields[7]),
                    "chi2_ndf": fnum(fields[8]),
                    "source_sps_path": str(path),
                }
            )
    return records


def read_summary_raws(path: Path) -> tuple[str | None, list[float]]:
    tray_id = None
    values: list[float] = []
    tray_re = re.compile(r"^Tray ID:\s*(.+?)\s*$")
    row_re = re.compile(
        r"^\s*(\d+)\s+([-+]?\d+(?:\.\d+)?)\s+([-+]?\d+(?:\.\d+)?)\s+([-+]?\d+(?:\.\d+)?)"
    )
    with path.open() as handle:
        for line in handle:
            tray_match = tray_re.match(line)
            if tray_match:
                tray_id = tray_match.group(1)
            row_match = row_re.match(line)
            if row_match:
                values.append(float(row_match.group(2)))
    return tray_id, values


# Correlations are hand-rolled rather than pulled from scipy so this script
# stays runnable with nothing but the standard library. They're checked against
# numpy and agree to ~1e-9.

def rank(values: list[float]) -> list[float]:
    # Ties take the average rank. Not a detail here: most chips have
    # damage_px == 0, so a third of the data is one giant tie group.
    indexed = sorted(enumerate(values), key=lambda item: item[1])
    ranks = [0.0] * len(values)
    i = 0
    while i < len(indexed):
        j = i + 1
        while j < len(indexed) and indexed[j][1] == indexed[i][1]:
            j += 1
        avg_rank = (i + 1 + j) / 2.0
        for k in range(i, j):
            ranks[indexed[k][0]] = avg_rank
        i = j
    return ranks


def pearson(xs: list[float], ys: list[float]) -> float | None:
    if len(xs) < 2 or len(xs) != len(ys):
        return None
    mx = mean(xs)
    my = mean(ys)
    dx = [x - mx for x in xs]
    dy = [y - my for y in ys]
    denom = math.sqrt(sum(x * x for x in dx) * sum(y * y for y in dy))
    if denom == 0:
        # Zero variance on one side. Return None, not 0.0, so an undefined
        # correlation can't show up in a table looking like "no correlation".
        return None
    return sum(x * y for x, y in zip(dx, dy)) / denom


def spearman(xs: list[float], ys: list[float]) -> float | None:
    # This is the one to quote. damage_frac is mostly zeros with a long right
    # tail, so a handful of badly scratched chips would drag Pearson around by
    # themselves.
    if len(xs) < 2 or len(xs) != len(ys):
        return None
    return pearson(rank(xs), rank(ys))


def fmt_corr(value: float | None) -> str:
    return "" if value is None else f"{value:.6g}"


def damage_class(row: dict) -> str:
    # Three buckets, not two. "Damage pixels" is the middle ground: the model
    # painted something but not enough to trip the threshold. Worth keeping
    # separate on the plot, since collapsing it into either side hides where
    # the cutoff is actually doing work.
    if row["damage_px"] == 0:
        return "Pristine"
    if row["predicted_damaged"]:
        return "CNN damaged"
    return "Damage pixels"


def collect_data(
    damage_rows: dict[tuple[str, int, int], dict],
    damage_trays: set[str],
    sps_roots: list[Path],
    aliases: dict[str, dict[str, str]],
    exclude_retests: bool,
    include_duplicate_sps: bool,
) -> tuple[list[dict], list[dict]]:
    merged: list[dict] = []
    audit: list[dict] = []
    seen_hashes: dict[str, str] = {}
    seen_measurements: dict[str, str] = {}
    sps_files: list[Path] = []
    for root in sps_roots:
        sps_files.extend(sorted(root.glob("*/debrecen/SPS_result_onlynumbers.txt")))
    if not sps_files:
        roots = ", ".join(str(root) for root in sps_roots)
        raise FileNotFoundError(f"No SPS_result_onlynumbers.txt files found under: {roots}")

    for sps_path in sps_files:
        source_root = next((root for root in sps_roots if root in sps_path.parents), sps_path.parents[2])
        source_collection = source_root.name
        folder_tray = sps_path.parents[1].name
        summary_path = sps_path.parents[1] / "summary" / "SPS summary"
        file_hash = hashlib.sha1(sps_path.read_bytes()).hexdigest()
        sps_records = read_sps_numbers(sps_path)
        for record in sps_records:
            record["source_collection"] = source_collection
            record["source_sps_root"] = str(source_root)
        sps_trays = sorted({record["sps_tray"] for record in sps_records})
        if len(sps_trays) != 1:
            raise ValueError(f"{sps_path} contains multiple tray IDs: {sps_trays}")
        sps_tray = sps_trays[0]
        source_sps_id_tray = sps_tray
        initial_status_bits: list[str] = []
        if sps_tray != folder_tray:
            initial_status_bits.append(
                f"SPS ID tray {sps_tray} differs from folder tray {folder_tray}"
            )
            if folder_tray in damage_trays:
                sps_tray = folder_tray
                for record in sps_records:
                    record["source_sps_id_tray"] = source_sps_id_tray
                    record["sps_tray"] = folder_tray
            else:
                for record in sps_records:
                    record["source_sps_id_tray"] = source_sps_id_tray
        else:
            for record in sps_records:
                record["source_sps_id_tray"] = source_sps_id_tray
        if exclude_retests and sps_tray.endswith("-retest"):
            audit.append(
                {
                    "sps_tray": sps_tray,
                    "damage_tray": "",
                    "match_type": "excluded_retest",
                    "sps_rows": len(sps_records),
                    "sps_field_counts": ",".join(str(v) for v in sorted({r["raw_sps_field_count"] for r in sps_records})),
                    "merged_rows": 0,
                    "missing_damage_rows": len(sps_records),
                    "summary_rows": "",
                    "summary_raw_max_abs_diff": "",
                    "status": "excluded by --exclude-retests",
                    "source_collection": source_collection,
                    "alias_note": "",
                    "source_sps_path": str(sps_path),
                    "source_summary_path": str(summary_path),
                }
            )
            continue

        damage_tray = sps_tray
        match_type = "exact"
        alias_note = ""
        if sps_tray in aliases:
            damage_tray = aliases[sps_tray]["damage_tray"]
            alias_note = aliases[sps_tray].get("note", "")
            match_type = "alias_to_damage"
            initial_status_bits.append(
                f"SPS tray {sps_tray} mapped to damage tray {damage_tray} via alias file"
            )
        if damage_tray not in damage_trays and sps_tray.endswith("-retest"):
            base = sps_tray.removesuffix("-retest")
            if base in damage_trays:
                damage_tray = base
                match_type = "retest_to_base_damage"
        elif damage_tray not in damage_trays:
            match_type = "no_damage_tray"

        summary_rows = ""
        summary_diff = ""
        status_bits: list[str] = list(initial_status_bits)
        if len(sps_records) != EXPECTED_PER_TRAY:
            status_bits.append(f"expected {EXPECTED_PER_TRAY} SPS rows")
        if summary_path.exists():
            summary_tray, summary_values = read_summary_raws(summary_path)
            summary_rows = str(len(summary_values))
            sorted_raws = [r["raw_vbd"] for r in sorted(sps_records, key=lambda r: (r["sps_i"], r["sps_j"]))]
            if len(summary_values) == len(sorted_raws):
                max_diff = max((abs(a - b) for a, b in zip(summary_values, sorted_raws)), default=0.0)
                summary_diff = f"{max_diff:.12g}"
                if max_diff > 1e-8:
                    status_bits.append("summary/raw mismatch")
            else:
                status_bits.append("summary row count mismatch")
            if summary_tray != sps_tray:
                status_bits.append("summary tray mismatch")
        else:
            status_bits.append("missing summary")

        duplicate_of = seen_hashes.get(file_hash)
        if duplicate_of and not include_duplicate_sps:
            status_bits.append(f"duplicate raw SPS file of {duplicate_of}; excluded")
            audit.append(
                {
                    "sps_tray": sps_tray,
                    "damage_tray": damage_tray if match_type != "no_damage_tray" else "",
                    "match_type": "duplicate_excluded",
                    "sps_rows": len(sps_records),
                    "sps_field_counts": ",".join(str(v) for v in sorted({r["raw_sps_field_count"] for r in sps_records})),
                    "merged_rows": 0,
                    "missing_damage_rows": 0,
                    "summary_rows": summary_rows,
                    "summary_raw_max_abs_diff": summary_diff,
                    "status": "; ".join(status_bits),
                    "source_collection": source_collection,
                    "alias_note": alias_note,
                    "source_sps_path": str(sps_path),
                    "source_summary_path": str(summary_path),
                }
            )
            continue
        seen_hashes[file_hash] = sps_tray

        duplicate_measurement = seen_measurements.get(sps_tray)
        if duplicate_measurement and not include_duplicate_sps:
            status_bits.append(f"duplicate SPS measurement tray already included from {duplicate_measurement}; excluded")
            audit.append(
                {
                    "sps_tray": sps_tray,
                    "damage_tray": damage_tray if match_type != "no_damage_tray" else "",
                    "match_type": "duplicate_measurement_excluded",
                    "sps_rows": len(sps_records),
                    "sps_field_counts": ",".join(str(v) for v in sorted({r["raw_sps_field_count"] for r in sps_records})),
                    "merged_rows": 0,
                    "missing_damage_rows": 0,
                    "summary_rows": summary_rows,
                    "summary_raw_max_abs_diff": summary_diff,
                    "status": "; ".join(status_bits),
                    "source_collection": source_collection,
                    "alias_note": alias_note,
                    "source_sps_path": str(sps_path),
                    "source_summary_path": str(summary_path),
                }
            )
            continue
        seen_measurements[sps_tray] = source_collection

        raw_mean = mean([r["raw_vbd"] for r in sps_records])
        v25_mean = mean([r["vbd_25c"] for r in sps_records])
        for record in sps_records:
            record["tray_mean_raw_vbd"] = raw_mean
            record["tray_mean_vbd_25c"] = v25_mean
            record["d_raw_vbd_mV"] = (raw_mean - record["raw_vbd"]) * 1000.0
            record["d_vbd_25c_mV"] = (v25_mean - record["vbd_25c"]) * 1000.0

        merged_count = 0
        missing_damage = 0
        for sps in sps_records:
            damage = damage_rows.get((damage_tray, sps["row"], sps["col"]))
            if damage is None:
                missing_damage += 1
                continue
            combined = {**sps, **damage}
            combined["damage_tray"] = damage_tray
            combined["damage_match_type"] = match_type
            combined["alias_note"] = alias_note
            combined["chip_label"] = f"R{sps['row']:02d}_C{sps['col']:02d}"
            combined["damage_percent"] = combined["damage_frac"] * 100.0
            combined["coverage_percent"] = combined["coverage_frac"] * 100.0
            combined["class"] = damage_class(combined)
            merged.append(combined)
            merged_count += 1
        if missing_damage:
            status_bits.append("missing damage rows")
        if match_type == "no_damage_tray":
            status_bits.append("no matching damage tray")
        if not status_bits:
            status_bits.append("ok")

        audit.append(
                {
                    "sps_tray": sps_tray,
                    "damage_tray": damage_tray if match_type != "no_damage_tray" else "",
                    "match_type": match_type,
                    "sps_rows": len(sps_records),
                    "sps_field_counts": ",".join(str(v) for v in sorted({r["raw_sps_field_count"] for r in sps_records})),
                    "merged_rows": merged_count,
                "missing_damage_rows": missing_damage,
                "summary_rows": summary_rows,
                    "summary_raw_max_abs_diff": summary_diff,
                    "status": "; ".join(status_bits),
                    "source_collection": source_collection,
                    "alias_note": alias_note,
                    "source_sps_path": str(sps_path),
                    "source_summary_path": str(summary_path),
                }
            )

    return merged, audit


def write_csv(path: Path, rows: list[dict], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def tray_summary_rows(merged: list[dict], audit: list[dict]) -> list[dict]:
    audit_by_tray = {row["sps_tray"]: row for row in audit}
    out: list[dict] = []
    by_tray: dict[str, list[dict]] = defaultdict(list)
    for row in merged:
        by_tray[row["sps_tray"]].append(row)
    for tray, rows in sorted(by_tray.items()):
        damage = [r["damage_frac"] for r in rows]
        y = [r["d_raw_vbd_mV"] for r in rows]
        nonzero = [r for r in rows if r["damage_px"] > 0]
        damaged = [r for r in rows if r["predicted_damaged"]]
        out.append(
            {
                "sps_tray": tray,
                "damage_tray": rows[0]["damage_tray"],
                "match_type": rows[0]["damage_match_type"],
                "n": len(rows),
                "nonzero_damage_n": len(nonzero),
                "cnn_damaged_n": len(damaged),
                "max_damage_percent": max([r["damage_percent"] for r in rows], default=0.0),
                "mean_raw_vbd": mean([r["raw_vbd"] for r in rows]),
                "sd_raw_vbd_mV": pstdev([r["raw_vbd"] for r in rows]) * 1000.0,
                "pearson_damage_vs_d_raw": pearson(damage, y),
                "spearman_damage_vs_d_raw": spearman(damage, y),
                "audit_status": audit_by_tray.get(tray, {}).get("status", ""),
            }
        )
    return out


def overall_stats(merged: list[dict]) -> dict:
    damage = [r["damage_frac"] for r in merged]
    y = [r["d_raw_vbd_mV"] for r in merged]
    nonzero = [r for r in merged if r["damage_px"] > 0]
    cnn = [r for r in merged if r["predicted_damaged"]]
    return {
        "points": len(merged),
        "sps_trays": len({r["sps_tray"] for r in merged}),
        "damage_trays": len({r["damage_tray"] for r in merged}),
        "nonzero_damage_points": len(nonzero),
        "cnn_damaged_points": len(cnn),
        "max_damage_percent": max([r["damage_percent"] for r in merged], default=0.0),
        "pearson_all": pearson(damage, y),
        "spearman_all": spearman(damage, y),
        "pearson_nonzero": pearson(
            [r["damage_frac"] for r in nonzero], [r["d_raw_vbd_mV"] for r in nonzero]
        ),
        "spearman_nonzero": spearman(
            [r["damage_frac"] for r in nonzero], [r["d_raw_vbd_mV"] for r in nonzero]
        ),
    }


def write_summary(path: Path, stats: dict, audit: list[dict]) -> None:
    problem_audits = [row for row in audit if row["status"] != "ok"]
    lines = [
        "SiPM Damage vs Raw SPS VBD Analysis",
        "",
        "Primary SPS value: Raw_VBD from debrecen/SPS_result_onlynumbers.txt",
        "Derived y value: tray_mean_raw_vbd - chip_raw_vbd, in mV",
        "Damage values are copied from damage_report.csv without scaling except percent display columns.",
        "",
        f"Merged points: {stats['points']}",
        f"SPS trays used: {stats['sps_trays']}",
        f"Damage trays represented: {stats['damage_trays']}",
        f"Nonzero-damage points: {stats['nonzero_damage_points']}",
        f"CNN-predicted damaged points: {stats['cnn_damaged_points']}",
        f"Max damage percent: {stats['max_damage_percent']:.6g}",
        f"Pearson damage_frac vs d_raw_vbd_mV, all points: {fmt_corr(stats['pearson_all'])}",
        f"Spearman damage_frac vs d_raw_vbd_mV, all points: {fmt_corr(stats['spearman_all'])}",
        f"Pearson damage_frac vs d_raw_vbd_mV, nonzero damage only: {fmt_corr(stats['pearson_nonzero'])}",
        f"Spearman damage_frac vs d_raw_vbd_mV, nonzero damage only: {fmt_corr(stats['spearman_nonzero'])}",
        "",
        "Audit notes:",
    ]
    if problem_audits:
        lines.extend(
            f"- {row['sps_tray']}: {row['status']} ({row['match_type']})" for row in problem_audits
        )
    else:
        lines.append("- all SPS files matched expected rows and summary raw values")
    path.write_text("\n".join(lines) + "\n")


def html_escape_json(data) -> str:
    return json.dumps(data, separators=(",", ":")).replace("</", "<\\/")


def build_html(path: Path, points: list[dict], tray_rows: list[dict], stats: dict) -> None:
    plot_points = [
        {
            "sps_tray": r["sps_tray"],
            "damage_tray": r["damage_tray"],
            "chip": r["chip_label"],
            "chip_file": r["chip"],
            "sps_id": r["sps_id"],
            "x": round(r["damage_percent"], 8),
            "coverage": round(r["coverage_percent"], 8),
            "y": round(r["d_raw_vbd_mV"], 8),
            "y25": round(r["d_vbd_25c_mV"], 8),
            "raw": round(r["raw_vbd"], 8),
            "v25": round(r["vbd_25c"], 8),
            "temp": round(r["avg_temp_c"], 5),
            "chi2": round(r["chi2_ndf"], 6),
            "pred": r["predicted_damaged"],
            "class": r["class"],
            "match": r["damage_match_type"],
        }
        for r in points
    ]
    trays = sorted({p["sps_tray"] for p in plot_points})
    max_x = max([p["x"] for p in plot_points], default=1.0)
    min_y = min([p["y"] for p in plot_points], default=-1.0)
    max_y = max([p["y"] for p in plot_points], default=1.0)
    tray_table = [
        {
            "tray": r["sps_tray"],
            "n": r["n"],
            "nonzero": r["nonzero_damage_n"],
            "cnn": r["cnn_damaged_n"],
            "maxDamage": round(r["max_damage_percent"], 5),
            "rho": None
            if r["spearman_damage_vs_d_raw"] is None
            else round(r["spearman_damage_vs_d_raw"], 4),
            "match": r["match_type"],
        }
        for r in tray_rows
    ]
    html_doc = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SiPM Damage vs Raw SPS VBD</title>
<style>
:root {{
  color-scheme: light;
  --ink: #1b1f23;
  --muted: #637083;
  --line: #d9dee8;
  --panel: #f7f8fb;
  --bg: #ffffff;
  --teal: #128c86;
  --blue: #2f6fd6;
  --orange: #c66a19;
  --red: #bf2f3f;
}}
* {{ box-sizing: border-box; }}
body {{
  margin: 0;
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  color: var(--ink);
  background: var(--bg);
}}
header {{
  padding: 22px 28px 14px;
  border-bottom: 1px solid var(--line);
}}
h1 {{
  margin: 0;
  font-size: 24px;
  font-weight: 720;
  letter-spacing: 0;
}}
.subtitle {{
  margin-top: 5px;
  color: var(--muted);
  font-size: 13px;
}}
.metrics {{
  display: grid;
  grid-template-columns: repeat(5, minmax(130px, 1fr));
  gap: 10px;
  padding: 14px 28px 8px;
}}
.metric {{
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 10px 12px;
  background: #fff;
}}
.metric b {{
  display: block;
  font-size: 19px;
  line-height: 1.2;
}}
.metric span {{
  color: var(--muted);
  font-size: 12px;
}}
.app {{
  display: grid;
  grid-template-columns: minmax(0, 1fr) 320px;
  gap: 16px;
  padding: 12px 28px 24px;
}}
.toolbar {{
  display: flex;
  flex-wrap: wrap;
  align-items: end;
  gap: 10px;
  margin-bottom: 10px;
}}
label {{
  display: grid;
  gap: 4px;
  color: var(--muted);
  font-size: 11px;
  font-weight: 650;
  text-transform: uppercase;
}}
select, input[type="search"] {{
  min-height: 34px;
  border: 1px solid var(--line);
  border-radius: 6px;
  background: #fff;
  color: var(--ink);
  padding: 5px 8px;
  font: inherit;
  min-width: 140px;
}}
.check {{
  display: flex;
  align-items: center;
  min-height: 34px;
  gap: 6px;
  color: var(--ink);
  font-size: 13px;
  text-transform: none;
  font-weight: 520;
}}
.plotwrap {{
  position: relative;
  min-height: 610px;
  border: 1px solid var(--line);
  border-radius: 8px;
  background: linear-gradient(#fff, #fff), var(--panel);
  overflow: hidden;
}}
canvas {{
  width: 100%;
  height: 610px;
  display: block;
}}
.tip {{
  position: absolute;
  pointer-events: none;
  min-width: 230px;
  max-width: 300px;
  border: 1px solid #c9d1df;
  border-radius: 8px;
  background: rgba(255,255,255,.97);
  box-shadow: 0 14px 36px rgba(31,45,61,.16);
  padding: 10px 11px;
  font-size: 12px;
  display: none;
}}
.tip strong {{ display: block; margin-bottom: 4px; font-size: 13px; }}
aside {{
  display: grid;
  gap: 12px;
  align-content: start;
}}
.panel {{
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 13px;
  background: #fff;
}}
.panel h2 {{
  font-size: 14px;
  margin: 0 0 8px;
}}
.kv {{
  display: grid;
  grid-template-columns: 110px minmax(0, 1fr);
  gap: 5px 9px;
  font-size: 12px;
}}
.kv span:nth-child(odd) {{ color: var(--muted); }}
table {{
  width: 100%;
  border-collapse: collapse;
  font-size: 12px;
}}
th, td {{
  padding: 5px 4px;
  border-bottom: 1px solid #edf0f5;
  text-align: right;
  white-space: nowrap;
}}
th:first-child, td:first-child {{ text-align: left; }}
th {{ color: var(--muted); font-weight: 680; }}
.legend {{
  display: flex;
  gap: 12px;
  align-items: center;
  color: var(--muted);
  font-size: 12px;
  padding: 7px 2px 0;
}}
.dot {{
  width: 9px;
  height: 9px;
  display: inline-block;
  border-radius: 50%;
  margin-right: 4px;
}}
@media (max-width: 980px) {{
  .metrics {{ grid-template-columns: repeat(2, minmax(130px, 1fr)); padding-left: 16px; padding-right: 16px; }}
  .app {{ grid-template-columns: 1fr; padding-left: 16px; padding-right: 16px; }}
  header {{ padding-left: 16px; padding-right: 16px; }}
  canvas {{ height: 520px; }}
  .plotwrap {{ min-height: 520px; }}
}}
</style>
</head>
<body>
<header>
  <h1>SiPM Damage vs Raw SPS VBD</h1>
  <div class="subtitle">x = CNN damage percent; y = tray mean Raw_VBD minus chip Raw_VBD. Positive y means lower-than-tray-average breakdown voltage.</div>
</header>
<section class="metrics">
  <div class="metric"><b>{stats["points"]:,}</b><span>joined chip points</span></div>
  <div class="metric"><b>{stats["sps_trays"]}</b><span>SPS measurements</span></div>
  <div class="metric"><b>{stats["nonzero_damage_points"]:,}</b><span>nonzero damage</span></div>
  <div class="metric"><b>{stats["cnn_damaged_points"]:,}</b><span>CNN damaged</span></div>
  <div class="metric"><b>{fmt_corr(stats["spearman_all"])}</b><span>Spearman, all points</span></div>
</section>
<main class="app">
  <section>
    <div class="toolbar">
      <label>Tray<select id="tray"><option value="__all__">All trays</option></select></label>
      <label>X Scale<select id="xscale"><option value="linear">Linear</option><option value="sqrt">Sqrt spread</option></select></label>
      <label>Color<select id="color"><option value="class">Damage class</option><option value="tray">Tray</option><option value="temp">Avg temp</option><option value="coverage">Coverage</option></select></label>
      <label>Search<input id="search" type="search" placeholder="R03_C14 or tray"></label>
      <label class="check"><input id="hideZero" type="checkbox"> hide zero damage</label>
      <label class="check"><input id="onlyCnn" type="checkbox"> CNN damaged only</label>
    </div>
    <div class="plotwrap">
      <canvas id="plot"></canvas>
      <div id="tip" class="tip"></div>
    </div>
    <div class="legend">
      <span><i class="dot" style="background:#9aa5b1"></i>Pristine</span>
      <span><i class="dot" style="background:#2f6fd6"></i>Damage pixels</span>
      <span><i class="dot" style="background:#bf2f3f"></i>CNN damaged</span>
    </div>
  </section>
  <aside>
    <div class="panel">
      <h2>Selected Point</h2>
      <div id="details" class="kv"><span>Point</span><span>hover the plot</span></div>
    </div>
    <div class="panel">
      <h2>Tray Summary</h2>
      <table id="trayTable">
        <thead><tr><th>Tray</th><th>n</th><th>Dmg</th><th>rho</th></tr></thead>
        <tbody></tbody>
      </table>
    </div>
  </aside>
</main>
<script>
const POINTS = {html_escape_json(plot_points)};
const TRAYS = {html_escape_json(trays)};
const TRAY_TABLE = {html_escape_json(tray_table)};
const DOMAIN = {{maxX:{max_x}, minY:{min_y}, maxY:{max_y}}};
window.SIPM_POINTS_COUNT = POINTS.length;
window.SIPM_TRAYS_COUNT = TRAYS.length;
const canvas = document.getElementById('plot');
const ctx = canvas.getContext('2d');
const tip = document.getElementById('tip');
const traySelect = document.getElementById('tray');
const xScaleSelect = document.getElementById('xscale');
const colorSelect = document.getElementById('color');
const searchInput = document.getElementById('search');
const hideZero = document.getElementById('hideZero');
const onlyCnn = document.getElementById('onlyCnn');
const details = document.getElementById('details');
const margin = {{left: 72, right: 28, top: 24, bottom: 62}};
let plotted = [];

for (const tray of TRAYS) {{
  const opt = document.createElement('option');
  opt.value = tray;
  opt.textContent = tray;
  traySelect.appendChild(opt);
}}

function niceTicks(min, max, count) {{
  const span = max - min || 1;
  const step0 = Math.pow(10, Math.floor(Math.log10(span / count)));
  const err = span / count / step0;
  const step = err >= 7.5 ? step0 * 10 : err >= 3.5 ? step0 * 5 : err >= 1.5 ? step0 * 2 : step0;
  const start = Math.ceil(min / step) * step;
  const ticks = [];
  for (let v = start; v <= max + step * .5; v += step) ticks.push(v);
  return ticks;
}}

function colorFor(p, mode) {{
  if (mode === 'class') {{
    if (p.class === 'CNN damaged') return '#bf2f3f';
    if (p.class === 'Damage pixels') return '#2f6fd6';
    return '#9aa5b1';
  }}
  if (mode === 'tray') {{
    let h = 0;
    for (let i = 0; i < p.sps_tray.length; i++) h = (h * 31 + p.sps_tray.charCodeAt(i)) % 360;
    return `hsl(${{h}}, 58%, 42%)`;
  }}
  if (mode === 'temp') {{
    const t = Math.max(18.6, Math.min(20.2, p.temp));
    const z = (t - 18.6) / 1.6;
    return `rgb(${{Math.round(35 + 195*z)}}, ${{Math.round(120 - 50*z)}}, ${{Math.round(190 - 90*z)}})`;
  }}
  const c = Math.min(1, p.coverage / Math.max(0.01, DOMAIN.maxX));
  return `rgb(${{Math.round(30 + 190*c)}}, ${{Math.round(130 - 55*c)}}, ${{Math.round(120 - 70*c)}})`;
}}

function filtered() {{
  const tray = traySelect.value;
  const q = searchInput.value.trim().toLowerCase();
  return POINTS.filter(p => {{
    if (tray !== '__all__' && p.sps_tray !== tray) return false;
    if (hideZero.checked && p.x === 0) return false;
    if (onlyCnn.checked && !p.pred) return false;
    if (q && !(p.chip.toLowerCase().includes(q) || p.sps_tray.toLowerCase().includes(q) || p.sps_id.toLowerCase().includes(q))) return false;
    return true;
  }});
}}

function xTransform(x) {{
  return xScaleSelect.value === 'sqrt' ? Math.sqrt(x) : x;
}}

function draw() {{
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  canvas.width = Math.max(320, Math.floor(rect.width * dpr));
  canvas.height = Math.max(320, Math.floor(rect.height * dpr));
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  const w = rect.width;
  const h = rect.height;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, w, h);
  const pts = filtered();
  const maxXRaw = Math.max(0.001, ...pts.map(p => p.x));
  const maxX = xTransform(maxXRaw);
  const ys = pts.map(p => p.y);
  let minY = Math.min(...ys, DOMAIN.minY);
  let maxY = Math.max(...ys, DOMAIN.maxY);
  const yPad = Math.max(5, (maxY - minY) * .08);
  minY -= yPad;
  maxY += yPad;
  const pw = w - margin.left - margin.right;
  const ph = h - margin.top - margin.bottom;
  const sx = x => margin.left + (xTransform(x) / maxX) * pw;
  const sy = y => margin.top + (1 - (y - minY) / (maxY - minY || 1)) * ph;

  ctx.strokeStyle = '#d9dee8';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(margin.left, margin.top);
  ctx.lineTo(margin.left, h - margin.bottom);
  ctx.lineTo(w - margin.right, h - margin.bottom);
  ctx.stroke();

  ctx.fillStyle = '#637083';
  ctx.font = '12px system-ui, sans-serif';
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';
  for (const t of niceTicks(minY, maxY, 7)) {{
    const y = sy(t);
    ctx.strokeStyle = '#edf0f5';
    ctx.beginPath();
    ctx.moveTo(margin.left, y);
    ctx.lineTo(w - margin.right, y);
    ctx.stroke();
    ctx.fillText(t.toFixed(0), margin.left - 8, y);
  }}
  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';
  const xTicks = xScaleSelect.value === 'sqrt'
    ? [0, maxXRaw*.01, maxXRaw*.04, maxXRaw*.16, maxXRaw*.36, maxXRaw*.64, maxXRaw].filter((v,i,a)=>i===0||v>a[i-1])
    : niceTicks(0, maxXRaw, 6);
  for (const t of xTicks) {{
    const x = sx(t);
    ctx.strokeStyle = '#edf0f5';
    ctx.beginPath();
    ctx.moveTo(x, margin.top);
    ctx.lineTo(x, h - margin.bottom);
    ctx.stroke();
    ctx.fillStyle = '#637083';
    ctx.fillText(t < 1 ? t.toFixed(2) : t.toFixed(1), x, h - margin.bottom + 8);
  }}

  ctx.save();
  ctx.translate(18, margin.top + ph / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.textAlign = 'center';
  ctx.fillStyle = '#1b1f23';
  ctx.font = '13px system-ui, sans-serif';
  ctx.fillText('Tray mean Raw_VBD - chip Raw_VBD (mV)', 0, 0);
  ctx.restore();
  ctx.textAlign = 'center';
  ctx.fillText('CNN damage percent of active area', margin.left + pw / 2, h - 22);

  plotted = [];
  const mode = colorSelect.value;
  for (const p of pts) {{
    const x = sx(p.x);
    const y = sy(p.y);
    const radius = p.pred ? 4.2 : p.x > 0 ? 3.2 : 2.0;
    ctx.globalAlpha = p.x > 0 ? .86 : .22;
    ctx.fillStyle = colorFor(p, mode);
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fill();
    plotted.push({{p, x, y, r: Math.max(radius + 3, 6)}});
  }}
  ctx.globalAlpha = 1;
  ctx.fillStyle = '#637083';
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  ctx.fillText(`${{pts.length.toLocaleString()}} points shown`, margin.left, 8);
  window.SIPM_PLOTTED_COUNT = plotted.length;
}}

function nearest(evt) {{
  const rect = canvas.getBoundingClientRect();
  const mx = evt.clientX - rect.left;
  const my = evt.clientY - rect.top;
  let best = null;
  let bestD = Infinity;
  for (const item of plotted) {{
    const dx = item.x - mx;
    const dy = item.y - my;
    const d = dx * dx + dy * dy;
    if (d < bestD && d <= item.r * item.r * 2.8) {{
      best = item;
      bestD = d;
    }}
  }}
  return best ? {{...best, mx, my}} : null;
}}

function pointHtml(p) {{
  return `<strong>${{p.sps_tray}} / ${{p.chip}}</strong>
  <div>damage: ${{p.x.toFixed(5)}}%</div>
  <div>coverage: ${{p.coverage.toFixed(5)}}%</div>
  <div>dRaw VBD: ${{p.y.toFixed(3)}} mV</div>
  <div>Raw VBD: ${{p.raw.toFixed(4)}} V</div>
  <div>Avg temp: ${{p.temp.toFixed(2)}} C</div>`;
}}

function setDetails(p) {{
  details.innerHTML = `
    <span>SPS tray</span><span>${{p.sps_tray}}</span>
    <span>Damage tray</span><span>${{p.damage_tray}}</span>
    <span>Chip</span><span>${{p.chip}}</span>
    <span>SPS ID</span><span>${{p.sps_id}}</span>
    <span>Damage</span><span>${{p.x.toFixed(6)}}%</span>
    <span>Coverage</span><span>${{p.coverage.toFixed(6)}}%</span>
    <span>dRaw VBD</span><span>${{p.y.toFixed(4)}} mV</span>
    <span>Raw VBD</span><span>${{p.raw.toFixed(5)}} V</span>
    <span>VBD 25C</span><span>${{p.v25.toFixed(5)}} V</span>
    <span>Class</span><span>${{p.class}}</span>`;
}}

canvas.addEventListener('mousemove', evt => {{
  const hit = nearest(evt);
  if (!hit) {{
    tip.style.display = 'none';
    return;
  }}
  tip.innerHTML = pointHtml(hit.p);
  tip.style.display = 'block';
  const wrap = canvas.parentElement.getBoundingClientRect();
  tip.style.left = Math.min(hit.mx + 14, wrap.width - 315) + 'px';
  tip.style.top = Math.max(8, hit.my - 20) + 'px';
  setDetails(hit.p);
}});
canvas.addEventListener('mouseleave', () => tip.style.display = 'none');

function fillTrayTable() {{
  const body = document.querySelector('#trayTable tbody');
  body.innerHTML = '';
  for (const r of TRAY_TABLE) {{
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${{r.tray}}</td><td>${{r.n}}</td><td>${{r.nonzero}}</td><td>${{r.rho ?? ''}}</td>`;
    body.appendChild(tr);
  }}
}}

for (const el of [traySelect, xScaleSelect, colorSelect, searchInput, hideZero, onlyCnn]) {{
  el.addEventListener('input', draw);
}}
window.addEventListener('resize', draw);
fillTrayTable();
draw();
</script>
</body>
</html>
"""
    path.write_text(html_doc)


def main() -> None:
    args = parse_args()
    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    damage_rows, damage_trays = read_damage(args.damage_csv)
    aliases = read_aliases(args.alias_csv)
    sps_roots = args.sps_root if args.sps_root else default_sps_roots(args.robot_root)
    merged, audit = collect_data(
        damage_rows,
        damage_trays,
        sps_roots,
        aliases,
        args.exclude_retests,
        args.include_duplicate_sps,
    )
    if not merged:
        raise RuntimeError("No SPS rows joined to damage rows.")

    merged_fields = [
        "sps_tray",
        "damage_tray",
        "damage_match_type",
        "alias_note",
        "source_collection",
        "source_sps_root",
        "source_sps_id_tray",
        "sps_id",
        "raw_sps_field_count",
        "chip",
        "chip_label",
        "row",
        "col",
        "sps_i",
        "sps_j",
        "raw_vbd",
        "raw_vbd_err",
        "avg_temp_c",
        "vbd_25c",
        "vbd_25c_err",
        "tray_mean_raw_vbd",
        "d_raw_vbd_mV",
        "tray_mean_vbd_25c",
        "d_vbd_25c_mV",
        "damage_px",
        "artifact_px",
        "coverage_px",
        "damage_frac",
        "damage_percent",
        "coverage_frac",
        "coverage_percent",
        "predicted_damaged",
        "class",
        "peaks_used",
        "fit_width",
        "chi2_ndf",
        "source_sps_path",
    ]
    audit_fields = [
        "sps_tray",
        "damage_tray",
        "match_type",
        "sps_rows",
        "sps_field_counts",
        "merged_rows",
        "missing_damage_rows",
        "summary_rows",
        "summary_raw_max_abs_diff",
        "status",
        "source_collection",
        "alias_note",
        "source_sps_path",
        "source_summary_path",
    ]
    tray_fields = [
        "sps_tray",
        "damage_tray",
        "match_type",
        "n",
        "nonzero_damage_n",
        "cnn_damaged_n",
        "max_damage_percent",
        "mean_raw_vbd",
        "sd_raw_vbd_mV",
        "pearson_damage_vs_d_raw",
        "spearman_damage_vs_d_raw",
        "audit_status",
    ]
    tray_rows = tray_summary_rows(merged, audit)
    stats = overall_stats(merged)

    write_csv(output_dir / "merged_sps_damage_raw.csv", merged, merged_fields)
    write_csv(output_dir / "sps_join_audit.csv", audit, audit_fields)
    write_csv(output_dir / "per_tray_summary.csv", tray_rows, tray_fields)
    write_summary(output_dir / "analysis_summary.txt", stats, audit)
    build_html(output_dir / "damage_vs_raw_sps_interactive.html", merged, tray_rows, stats)

    print(f"Wrote {output_dir / 'merged_sps_damage_raw.csv'}")
    print(f"Wrote {output_dir / 'sps_join_audit.csv'}")
    print(f"Wrote {output_dir / 'per_tray_summary.csv'}")
    print(f"Wrote {output_dir / 'analysis_summary.txt'}")
    print(f"Wrote {output_dir / 'damage_vs_raw_sps_interactive.html'}")
    print("SPS roots:")
    for root in sps_roots:
        print(f"  {root}")
    if aliases:
        print(f"Alias mappings loaded: {len(aliases)} from {args.alias_csv}")
    print(f"Joined points: {stats['points']} across {stats['sps_trays']} SPS measurements")
    print(f"Spearman all: {fmt_corr(stats['spearman_all'])}")


if __name__ == "__main__":
    main()

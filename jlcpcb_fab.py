#!/usr/bin/env python3
"""Generate a JLCPCB-ready gerber + drill zip from a KiCad project."""
from __future__ import annotations

import argparse
import csv
import re
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

FAB_LAYERS = [
    "F.Paste", "B.Paste",
    "F.Silkscreen", "B.Silkscreen",
    "F.Mask", "B.Mask",
    "Edge.Cuts",
]


def find_one(project_dir: Path, suffix: str) -> Path:
    hits = list(project_dir.glob(f"*{suffix}"))
    if len(hits) != 1:
        sys.exit(f"expected exactly one *{suffix} in {project_dir}, found {len(hits)}")
    return hits[0]


def detect_copper_layers(pcb: Path) -> list[str]:
    """Parse the (layers ...) block and return signal-layer names in order."""
    text = pcb.read_text()
    match = re.search(r"\(layers\b(.*?)^\s*\)", text, re.DOTALL | re.MULTILINE)
    if not match:
        sys.exit(f"could not find (layers ...) block in {pcb}")
    layers = re.findall(r'"([^"]+)"\s+signal\b', match.group(1))
    if not layers:
        sys.exit(f"no signal (copper) layers found in {pcb}")
    return layers


def run_kicad(args: list[str]) -> None:
    print("==>", " ".join(args))
    subprocess.run(["kicad-cli", *args], check=True)


def export_bom(sch: Path, out_path: Path) -> None:
    """Export a JLCPCB-format BOM: Comment, Designator, Footprint, LCSC Part #."""
    run_kicad([
        "sch", "export", "bom",
        "--output", str(out_path),
        "--fields", "Value,Reference,Footprint,LCSC",
        "--labels", "Comment,Designator,Footprint,LCSC Part #",
        "--group-by", "Value,Footprint,LCSC",
        "--exclude-dnp",
        str(sch),
    ])


def export_cpl(pcb: Path, out_path: Path) -> None:
    """Export a JLCPCB-format CPL: Designator, Mid X, Mid Y, Layer, Rotation."""
    with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as tmp:
        raw = Path(tmp.name)
    try:
        run_kicad([
            "pcb", "export", "pos",
            "--output", str(raw),
            "--side", "both",
            "--format", "csv",
            "--units", "mm",
            "--use-drill-file-origin",
            "--exclude-dnp",
            str(pcb),
        ])
        with raw.open(newline="") as fin:
            rows = list(csv.DictReader(fin))
        with out_path.open("w", newline="") as fout:
            writer = csv.writer(fout)
            writer.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
            for r in rows:
                writer.writerow([r["Ref"], r["PosX"], r["PosY"], r["Side"], r["Rot"]])
    finally:
        raw.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project_dir", type=Path)
    opts = parser.parse_args()

    project_dir = opts.project_dir.resolve()
    pcb = find_one(project_dir, ".kicad_pcb")
    sch = find_one(project_dir, ".kicad_sch")
    name = pcb.stem
    out_dir = project_dir / "fab"
    out_dir.mkdir(exist_ok=True)
    zip_path = out_dir / f"{name}_jlcpcb.zip"
    bom_path = out_dir / f"{name}_bom.csv"
    cpl_path = out_dir / f"{name}_cpl.csv"

    copper = detect_copper_layers(pcb)
    layers = ",".join(copper + FAB_LAYERS)
    print(f"==> layers: {layers}")

    with tempfile.TemporaryDirectory(prefix=f"jlcpcb_{name}_") as tmp:
        work_dir = Path(tmp)
        run_kicad([
            "pcb", "export", "gerbers",
            "--output", f"{work_dir}/",
            "--layers", layers,
            "--no-x2",
            "--subtract-soldermask",
            str(pcb),
        ])
        run_kicad([
            "pcb", "export", "drill",
            "--output", f"{work_dir}/",
            "--format", "excellon",
            "--drill-origin", "absolute",
            "--excellon-units", "mm",
            "--excellon-zeros-format", "decimal",
            "--excellon-oval-format", "alternate",
            str(pcb),
        ])

        zip_path.unlink(missing_ok=True)
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
            for f in sorted(work_dir.iterdir()):
                if f.is_file():
                    z.write(f, f.name)

    export_bom(sch, bom_path)
    export_cpl(pcb, cpl_path)
    print(f"done: {zip_path}")
    print(f"done: {bom_path}")
    print(f"done: {cpl_path}")


if __name__ == "__main__":
    main()

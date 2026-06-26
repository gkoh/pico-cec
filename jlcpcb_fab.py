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
    "F.Paste",
    "B.Paste",
    "F.Silkscreen",
    "B.Silkscreen",
    "F.Mask",
    "B.Mask",
    "Edge.Cuts",
]


def find_one(project_dir: Path, suffix: str) -> Path:
    hits = list(project_dir.glob(f"*{suffix}"))
    if len(hits) != 1:
        sys.exit(f"expected exactly one *{suffix} in {project_dir}, found {len(hits)}")
    return hits[0]


def discover_projects(root: Path) -> list[Path]:
    """Return board project dirs under root (a dir is a board if it holds a .kicad_pcb).

    If root itself is a board project, return it directly; otherwise scan its
    immediate subdirectories. Shared dirs like ``library/`` or ``*-backups/`` carry
    no top-level .kicad_pcb and are skipped automatically.
    """
    if list(root.glob("*.kicad_pcb")):
        return [root]
    projects = sorted(
        d for d in root.iterdir() if d.is_dir() and list(d.glob("*.kicad_pcb"))
    )
    if not projects:
        sys.exit(
            f"no board projects (*.kicad_pcb) found in {root} or its subdirectories"
        )
    return projects


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
    run_kicad(
        [
            "sch",
            "export",
            "bom",
            "--output",
            str(out_path),
            "--fields",
            "Value,Reference,Footprint,LCSC",
            "--labels",
            "Comment,Designator,Footprint,LCSC Part #",
            "--group-by",
            "Value,Footprint,LCSC",
            "--exclude-dnp",
            str(sch),
        ]
    )


def export_schematic_pdf(sch: Path, out_path: Path) -> None:
    """Export the schematic as a PDF."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    run_kicad(["sch", "export", "pdf", "--output", str(out_path), str(sch)])


def render_board(pcb: Path, out_path: Path) -> None:
    """Render an isometric 3D view of the board to a PNG under docs/."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    run_kicad(
        [
            "pcb",
            "render",
            "--output",
            str(out_path),
            "--width",
            "1024",
            "--height",
            "1024",
            "--rotate",
            "-45,0,225",
            "--zoom",
            "0.8",
            "--quality",
            "high",
            "--perspective",
            "--floor",
            str(pcb),
        ]
    )


def export_cpl(pcb: Path, out_path: Path) -> None:
    """Export a JLCPCB-format CPL: Designator, Mid X, Mid Y, Layer, Rotation."""
    with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as tmp:
        raw = Path(tmp.name)
    try:
        run_kicad(
            [
                "pcb",
                "export",
                "pos",
                "--output",
                str(raw),
                "--side",
                "both",
                "--format",
                "csv",
                "--units",
                "mm",
                "--use-drill-file-origin",
                "--exclude-dnp",
                str(pcb),
            ]
        )
        with raw.open(newline="") as fin:
            rows = list(csv.DictReader(fin))
        with out_path.open("w", newline="") as fout:
            writer = csv.writer(fout)
            writer.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
            for r in rows:
                writer.writerow([r["Ref"], r["PosX"], r["PosY"], r["Side"], r["Rot"]])
    finally:
        raw.unlink(missing_ok=True)


def process_project(project_dir: Path) -> None:
    pcb = find_one(project_dir, ".kicad_pcb")
    sch = find_one(project_dir, ".kicad_sch")
    name = pcb.stem
    out_dir = project_dir / "fab"
    out_dir.mkdir(exist_ok=True)
    zip_path = out_dir / f"{name}_jlcpcb.zip"
    bom_path = out_dir / f"{name}_bom.csv"
    cpl_path = out_dir / f"{name}_cpl.csv"
    docs_dir = Path(__file__).resolve().parent / "docs"
    pdf_path = docs_dir / f"{name}_schematic.pdf"
    render_path = docs_dir / f"{name}.png"

    copper = detect_copper_layers(pcb)
    layers = ",".join(copper + FAB_LAYERS)
    print(f"==> {name}: layers: {layers}")

    with tempfile.TemporaryDirectory(prefix=f"jlcpcb_{name}_") as tmp:
        work_dir = Path(tmp)
        run_kicad(
            [
                "pcb",
                "export",
                "gerbers",
                "--output",
                f"{work_dir}/",
                "--layers",
                layers,
                "--no-x2",
                "--subtract-soldermask",
                str(pcb),
            ]
        )
        run_kicad(
            [
                "pcb",
                "export",
                "drill",
                "--output",
                f"{work_dir}/",
                "--format",
                "excellon",
                "--drill-origin",
                "absolute",
                "--excellon-units",
                "mm",
                "--excellon-zeros-format",
                "decimal",
                "--excellon-oval-format",
                "alternate",
                str(pcb),
            ]
        )

        zip_path.unlink(missing_ok=True)
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
            for f in sorted(work_dir.iterdir()):
                if f.is_file():
                    z.write(f, f.name)

    export_bom(sch, bom_path)
    export_cpl(pcb, cpl_path)
    export_schematic_pdf(sch, pdf_path)
    render_board(pcb, render_path)
    print(f"done: {zip_path}")
    print(f"done: {bom_path}")
    print(f"done: {cpl_path}")
    print(f"done: {pdf_path}")
    print(f"done: {render_path}")


def default_pcb_root() -> Path:
    """Repo's pcb/ directory if present, else the script's own directory."""
    here = Path(__file__).resolve().parent
    pcb = here / "pcb"
    return pcb if pcb.is_dir() else here


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "project_dir",
        type=Path,
        nargs="?",
        default=default_pcb_root(),
        help="a board project dir, or a parent dir of board projects "
        "(default: the repo's pcb/ directory)",
    )
    opts = parser.parse_args()

    projects = discover_projects(opts.project_dir.resolve())
    print(f"==> board projects: {', '.join(p.name for p in projects)}")
    for project_dir in projects:
        process_project(project_dir)


if __name__ == "__main__":
    main()

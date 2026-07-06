#!/usr/bin/env python3
"""
parse_size.py -- report the flash / static-RAM footprint of STM32_Bare_Test
builds from their ELF section sizes, and quantify the callback-only flash
savings.

For every build/<board>-<build>-<target>-<config>/app.elf (the Makefile
BUILD_DIR layout) we run arm-none-eabi-size and record:
    .text  -- code + const (flash)
    .data  -- initialized RAM, whose image also lives in flash
    .bss   -- zero-initialized RAM
Flash image = .text + .data; static RAM = .data + .bss (heap and stack are
measured separately by parse_bench.py's STACK=1 tables).

Two sections are emitted:
  1. A per-build static-footprint table.
  2. A "callback-only flash savings" table comparing each TARGET=cbonly build
     to the same board/build/config at a reference target (default: dhuk,
     which carries the same DHUK crypto surface without the STM32_BARE_CB_ONLY
     software-strip preset). The delta is the flash removed by running the four
     primitives on hardware instead of in software.

Whole-image static only: per-algorithm static isolation would need single-algo
build variants, which do not exist in this harness. Build the variants you want
to compare with `make ... TARGET=cbonly` / `TARGET=dhuk` (optionally `-k` via
bench_matrix.sh to keep the build dirs), then run this from anywhere.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

BUILD_DIR_DEFAULT = Path(__file__).resolve().parent / "build"

# Berkeley `size` output: "   text    data     bss     dec     hex filename"
RE_SIZE = re.compile(
    r"^\s*(?P<text>\d+)\s+(?P<data>\d+)\s+(?P<bss>\d+)\s+\d+\s+[0-9a-fA-F]+",
    re.M,
)


def elf_size(size_tool: str, elf: Path):
    """Return (text, data, bss) or None."""
    try:
        out = subprocess.run([size_tool, str(elf)], capture_output=True,
                             text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError) as e:
        print(f"warning: {size_tool} failed for {elf}: {e}", file=sys.stderr)
        return None
    m = RE_SIZE.search(out)
    if not m:
        print(f"warning: could not parse size for {elf}", file=sys.stderr)
        return None
    return (int(m.group("text")), int(m.group("data")), int(m.group("bss")))


def parse_dir(name: str):
    """Split <board>-<build>-<target>-<config>. Board/build/target/config
    never contain '-', so a 4-way split is exact."""
    parts = name.split("-")
    if len(parts) != 4:
        return None
    return tuple(parts)  # (board, build, target, config)


def collect(build_dir: Path, size_tool: str) -> dict:
    rows: dict = {}
    for d in sorted(build_dir.glob("*")):
        elf = d / "app.elf"
        if not elf.exists():
            continue
        key = parse_dir(d.name)
        if key is None:
            continue
        sz = elf_size(size_tool, elf)
        if sz is not None:
            rows[key] = sz
    return rows


def fmt(n: int) -> str:
    return f"{n:,}"


def render_table(rows: dict) -> str:
    lines = [
        "### Static footprint (bytes)",
        "",
        "From `arm-none-eabi-size` on each "
        "`build/<board>-<build>-<target>-<config>/app.elf`. Flash = `.text` + "
        "`.data`; static RAM = `.data` + `.bss` (heap/stack are separate -- see "
        "the STACK=1 bench mem tables).",
        "",
        "| Board | Build | Target | Config | .text | .data | .bss | Flash | "
        "Static RAM |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    for key in sorted(rows):
        board, build, target, config = key
        text, data, bss = rows[key]
        lines.append(
            f"| `{board}` | {build} | {target} | {config} | {fmt(text)} | "
            f"{fmt(data)} | {fmt(bss)} | {fmt(text + data)} | "
            f"{fmt(data + bss)} |"
        )
    return "\n".join(lines)


def render_savings(rows: dict, ref: str) -> str:
    out = []
    for key in sorted(rows):
        board, build, target, config = key
        if target != "cbonly":
            continue
        rkey = (board, build, ref, config)
        if rkey not in rows:
            continue
        text, data, _ = rows[key]
        rtext, rdata, _ = rows[rkey]
        cb_flash = text + data
        ref_flash = rtext + rdata
        delta = ref_flash - cb_flash
        pct = (100.0 * delta / ref_flash) if ref_flash else 0.0
        out.append((board, build, config, ref_flash, cb_flash, delta, pct))
    if not out:
        return ""
    lines = [
        "### Callback-only flash savings",
        "",
        f"Flash image of `TARGET=cbonly` (STM32_BARE_CB_ONLY preset, four "
        f"primitives on hardware) vs the same board/build/config at "
        f"`TARGET={ref}` (full software crypto surface). Positive `Saved` is "
        "flash removed by dropping the software implementations the crypto "
        "callback replaces.",
        "",
        f"| Board | Build | Config | {ref} flash | cbonly flash | Saved | "
        "Saved % |",
        "|---|---|---|---|---|---|---|",
    ]
    for board, build, config, rf, cf, d, pct in out:
        lines.append(
            f"| `{board}` | {build} | {config} | {fmt(rf)} | {fmt(cf)} | "
            f"{fmt(d)} | {pct:.1f}% |"
        )
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-dir", default=str(BUILD_DIR_DEFAULT),
                    help="path to build/ (default: alongside this script)")
    ap.add_argument("--size-tool", default="arm-none-eabi-size",
                    help="size binary (default: arm-none-eabi-size)")
    ap.add_argument("--ref-target", default="dhuk",
                    help="target to compare cbonly against (default: dhuk)")
    args = ap.parse_args()
    build_dir = Path(args.build_dir)
    if not build_dir.exists():
        print(f"error: build dir {build_dir} does not exist", file=sys.stderr)
        return 2
    rows = collect(build_dir, args.size_tool)
    if not rows:
        print(f"error: no build/<dir>/app.elf found in {build_dir}",
              file=sys.stderr)
        return 2
    print(render_table(rows))
    savings = render_savings(rows, args.ref_target)
    if savings:
        print()
        print(savings)
    return 0


if __name__ == "__main__":
    sys.exit(main())

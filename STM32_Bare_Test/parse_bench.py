#!/usr/bin/env python3
"""
parse_bench.py -- consume STM32_Bare_Test/bench_logs/<board>_<config>.log
(or <board>_cubemx_<config>.log for the cubemx build axis) and emit
markdown bench tables for the README.

The wolfcrypt bench output format we parse (one line per algo):
    AES-128-CBC-enc    20 MB took 1.012 seconds,   19.764 MB/s
    AES-256-GCM-enc     8 MB took 1.207 seconds,    6.628 MB/s
    SHA-256            45 MB took 1.000 seconds,   44.998 MB/s
    RNG                23 MB took 1.001 seconds,   22.978 MB/s
    ECC   [SECP256R1]   256 key gen  20 ops took 1.018 sec,  19.646 ops/sec
    ECDSA [SECP256R1]   256 sign     14 ops took 1.034 sec,  13.539 ops/sec
    ECDSA [SECP256R1]   256 verify    8 ops took 1.220 sec,   6.557 ops/sec

The log stem is <board>_<config>, or <board>_cubemx_<config> when
bench_matrix.sh runs with -B cubemx (the build axis is inserted as a
middle token). Board names never contain '_', so we split the board off
the front and treat the remainder as the config key (bare/asm/c or
cubemx_bare/cubemx_asm/cubemx_c). A trailing _stack token marks the
STACK=1 companion run (<board>_<config>_stack): its memory metrics are
merged onto the matching (board, config) so perf comes from the STACK=0
run and stack/heap come from the STACK=1 run.

For each (board, config) we extract:
    - AES-128-CBC enc / dec MB/s
    - AES-128-GCM enc / dec MB/s
    - SHA-256 MB/s
    - RNG MB/s
    - ECDSA P-256 sign / verify ops/sec

If the bench was built with STACK=1, each bench line also carries the
optional suffix:
    AES-128-CBC-enc ... MiB/s [heap 0 bytes (0 allocs), stack 368 bytes]
and a cumulative summary block is emitted at the end:
    total   Allocs   =      1339
    total   Deallocs =      1339
    total   Bytes    =    173248
    peak    Bytes    =      2368

When that data is present we additionally emit a stack-per-algo table,
a heap-per-algo table, and a one-row peak-heap summary table.

Then we emit two markdown tables: symmetric (MB/s) and asymmetric (ops/sec).

Run from anywhere; logs are located via the script's own directory.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

LOG_DIR_DEFAULT = Path(__file__).resolve().parent / "bench_logs"

CONFIGS = ("bare", "asm", "c", "cubemx_bare", "cubemx_asm", "cubemx_c")

BOARD_ORDER = [
    "c031", "c562", "c5a3", "f207", "f303", "f437", "f439", "f767",
    "g071", "g474", "g491", "h5", "h573", "h7", "h723", "h7a3", "h7s3",
    "l4a6", "l552", "l562", "n657", "u083", "u3", "u5", "u545",
    "u585", "wb55", "wba52", "wl55",
]

SYMM_KEYS = [
    ("aes128_cbc_enc", "AES-128-CBC enc"),
    ("aes128_cbc_dec", "AES-128-CBC dec"),
    ("aes128_gcm_enc", "AES-128-GCM enc"),
    ("aes128_gcm_dec", "AES-128-GCM dec"),
    ("sha256",        "SHA-256"),
    ("rng",           "RNG"),
]

ASYM_KEYS = [
    ("ecdsa_p256_sign",   "ECDSA P-256 sign"),
    ("ecdsa_p256_verify", "ECDSA P-256 verify"),
]

# Mem-tracking algos (subset; STACK=1 builds report per-algo stack + heap).
MEM_KEYS = [
    ("aes128_cbc_enc",    "AES-128-CBC enc"),
    ("aes128_gcm_enc",    "AES-128-GCM enc"),
    ("aes128_gcm_dec",    "AES-128-GCM dec"),
    ("sha256",            "SHA-256"),
    ("ecdsa_p256_sign",   "ECDSA P-256 sign"),
    ("ecdsa_p256_verify", "ECDSA P-256 verify"),
]

# Regex pieces ------------------------------------------------------------
# The optional " [heap N bytes (M allocs), stack S bytes]" suffix appears
# when the bench was built with STACK=1 (-DSTM32_BARE_STACK_TRACK).
RE_MEM_SUFFIX = (
    r"(?:\s*\[heap\s+(?P<heap>\d+)\s+bytes\s+\((?P<allocs>\d+)\s+allocs\),"
    r"\s+stack\s+(?P<stack>\d+)\s+bytes\])?"
)
RE_MB_S = re.compile(
    r"^\s*(?P<algo>\S[\S\- ]*?)\s+\d+\s+(?:MiB|KiB|MB|KB|B)\s+took\s+[\d.]+\s+seconds?,\s+"
    r"(?P<rate>[\d.]+)\s+(?P<unit>MiB|KiB|MB|KB)/s"
    + RE_MEM_SUFFIX,
    re.IGNORECASE,
)
RE_OPS = re.compile(
    r"^\s*(?P<algo>\S[\S\- ]*?)\s+\d+\s+ops\s+took\s+[\d.]+\s+sec,?\s+"
    r"(?:avg\s+[\d.]+\s+ms,)?\s*(?P<rate>[\d.]+)\s+ops/sec"
    + RE_MEM_SUFFIX,
    re.IGNORECASE,
)
RE_PEAK_BYTES = re.compile(r"^\s*peak\s+Bytes\s*=\s*(?P<peak>\d+)", re.IGNORECASE)


def normalize_algo(s: str) -> str:
    s = s.strip().lower()
    s = re.sub(r"\s+", " ", s)
    return s


def to_mb_per_sec(rate: float, unit: str) -> float:
    u = unit.upper()
    if u in ("MIB", "MB"):
        return rate
    return rate / 1024.0  # KiB/KB -> MiB/MB


def parse_log(path: Path) -> dict:
    """Return dict of metric_key -> float (MB/s or ops/sec).

    When the bench was built with STACK=1, additional keys "<key>_stack"
    and "<key>_heap" are present (integer bytes), plus "peak_bytes"
    (cumulative concurrent heap high-water at end of bench).
    """
    out: dict = {}
    if not path.exists():
        return out
    text = path.read_text(errors="replace").splitlines()
    for line in text:
        m = RE_MB_S.match(line)
        if m:
            algo = normalize_algo(m.group("algo"))
            rate = to_mb_per_sec(float(m.group("rate")), m.group("unit"))
            _stash_symm(out, algo, rate)
            _stash_mem(out, algo, m)
            continue
        m = RE_OPS.match(line)
        if m:
            algo = normalize_algo(m.group("algo"))
            rate = float(m.group("rate"))
            _stash_asym(out, algo, rate)
            _stash_mem(out, algo, m)
            continue
        m = RE_PEAK_BYTES.match(line)
        if m:
            out["peak_bytes"] = int(m.group("peak"))
    return out


def _stash_symm(out: dict, algo: str, rate: float) -> None:
    if algo.startswith("aes-128-cbc-enc"):
        out["aes128_cbc_enc"] = rate
    elif algo.startswith("aes-128-cbc-dec"):
        out["aes128_cbc_dec"] = rate
    elif algo.startswith("aes-128-gcm-enc"):
        out["aes128_gcm_enc"] = rate
    elif algo.startswith("aes-128-gcm-dec"):
        out["aes128_gcm_dec"] = rate
    elif algo.startswith("sha-256") or algo == "sha256":
        out["sha256"] = rate
    elif algo.startswith("rng"):
        out["rng"] = rate


def _stash_asym(out: dict, algo: str, rate: float) -> None:
    if "ecdsa" in algo and ("secp256r1" in algo or "p-256" in algo or "256" in algo):
        if "sign" in algo:
            out["ecdsa_p256_sign"] = rate
        elif "verify" in algo:
            out["ecdsa_p256_verify"] = rate


def _stash_mem(out: dict, algo: str, m) -> None:
    """Stash per-algo stack/heap bytes from a regex match if the suffix
    captured. Keys mirror the perf keys with '_stack'/'_heap' suffix."""
    stack = m.groupdict().get("stack")
    heap  = m.groupdict().get("heap")
    if stack is None and heap is None:
        return
    base = None
    if algo.startswith("aes-128-cbc-enc"):    base = "aes128_cbc_enc"
    elif algo.startswith("aes-128-cbc-dec"):  base = "aes128_cbc_dec"
    elif algo.startswith("aes-128-gcm-enc"):  base = "aes128_gcm_enc"
    elif algo.startswith("aes-128-gcm-dec"):  base = "aes128_gcm_dec"
    elif algo.startswith("sha-256") or algo == "sha256":
        base = "sha256"
    elif algo.startswith("rng"):
        base = "rng"
    elif "ecdsa" in algo and "256" in algo:
        if "sign" in algo:    base = "ecdsa_p256_sign"
        elif "verify" in algo: base = "ecdsa_p256_verify"
    if base is None:
        return
    if stack is not None:
        out[base + "_stack"] = int(stack)
    if heap is not None:
        out[base + "_heap"] = int(heap)


def collect(log_dir: Path) -> dict:
    """Return {board: {config: {metric: value}}}.

    Log stems: <board>_<cfg>[.log] for perf (STACK=0), and an optional
    <board>_<cfg>_stack[.log] (STACK=1) whose per-algo stack/heap and peak
    metrics are merged onto the same (board, cfg) -- so a config's perf comes
    from its STACK=0 run and its memory numbers from its STACK=1 run. The
    optional cubemx build-axis token is part of <cfg> (e.g. cubemx_bare).
    """
    data: dict = defaultdict(lambda: defaultdict(dict))
    for log in sorted(log_dir.glob("*.log")):
        name = log.stem  # board_cfg, board_cubemx_cfg, optionally + _stack
        if "_" not in name:
            continue
        is_stack = name.endswith("_stack")
        core = name[:-len("_stack")] if is_stack else name
        # Board names never contain '_', so split off the front; the
        # remainder is the config key (bare/asm/c or cubemx_bare/...).
        board, _, cfg = core.partition("_")
        if cfg not in CONFIGS:
            continue
        parsed = parse_log(log)
        dest = data[board][cfg]
        if is_stack:
            # Merge only the memory metrics; keep perf from the STACK=0 run.
            for k, v in parsed.items():
                if k.endswith("_stack") or k.endswith("_heap") \
                        or k == "peak_bytes":
                    dest[k] = v
        else:
            dest.update(parsed)
    return data


def fmt_cell(v) -> str:
    if v is None:
        return "-"
    if isinstance(v, float):
        if v >= 100:
            return f"{v:.1f}"
        if v >= 10:
            return f"{v:.2f}"
        return f"{v:.3f}"
    return str(v)


def render_symm(data: dict) -> str:
    lines = []
    lines.append("### Symmetric benchmark (MiB/s)")
    lines.append("")
    lines.append("Captured by `bench_matrix.sh`. Each cell is the wolfcrypt "
                 "`TARGET=bench` report for that algorithm on that silicon "
                 "in the listed build flavor. `-` means the run was skipped, "
                 "failed, or the algorithm did not appear in the bench output.")
    lines.append("")
    header = ["Board"]
    for _, label in SYMM_KEYS:
        for cfg in CONFIGS:
            header.append(f"{label} ({cfg})")
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "|".join(["---"] * len(header)) + "|")
    for board in BOARD_ORDER:
        if board not in data:
            continue
        row = [f"`{board}`"]
        for key, _ in SYMM_KEYS:
            for cfg in CONFIGS:
                v = data[board].get(cfg, {}).get(key)
                row.append(fmt_cell(v))
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def render_asym(data: dict) -> str:
    lines = []
    lines.append("### Asymmetric benchmark (ops/sec)")
    lines.append("")
    lines.append("ECDSA P-256 sign/verify only -- the most commonly accelerated "
                 "asymmetric operation. Boards with HW PKA in their accelerator "
                 "table should show a step-up vs `c` config; boards without PKA "
                 "should show roughly identical numbers across configs (SW math).")
    lines.append("")
    header = ["Board"]
    for _, label in ASYM_KEYS:
        for cfg in CONFIGS:
            header.append(f"{label} ({cfg})")
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "|".join(["---"] * len(header)) + "|")
    for board in BOARD_ORDER:
        if board not in data:
            continue
        row = [f"`{board}`"]
        for key, _ in ASYM_KEYS:
            for cfg in CONFIGS:
                v = data[board].get(cfg, {}).get(key)
                row.append(fmt_cell(v))
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def _has_mem_data(data: dict) -> bool:
    for board, cfgs in data.items():
        for cfg, metrics in cfgs.items():
            for k in metrics:
                if k.endswith("_stack") or k.endswith("_heap") or k == "peak_bytes":
                    return True
    return False


def render_stack(data: dict) -> str:
    lines = []
    lines.append("### Stack usage per algorithm (bytes)")
    lines.append("")
    lines.append("From STACK=1 builds. Each cell is the peak relative stack "
                 "consumption inside one bench iteration of that algorithm "
                 "(wolfssl `HAVE_STACK_SIZE_VERBOSE`). The carved stack "
                 "region per board is set by `_Min_Stack_Size` in the linker "
                 "script. AES algorithms with `NO_WOLFSSL_SMALL_STACK` "
                 "should be near-constant across boards; ECC/RSA varies with "
                 "SP-math precomp tables and HW PKA usage.")
    lines.append("")
    header = ["Board"]
    for _, label in MEM_KEYS:
        for cfg in CONFIGS:
            header.append(f"{label} ({cfg})")
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "|".join(["---"] * len(header)) + "|")
    for board in BOARD_ORDER:
        if board not in data:
            continue
        row = [f"`{board}`"]
        for key, _ in MEM_KEYS:
            for cfg in CONFIGS:
                v = data[board].get(cfg, {}).get(key + "_stack")
                row.append(fmt_cell(v))
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def render_heap(data: dict) -> str:
    lines = []
    lines.append("### Heap usage per algorithm (bytes per iteration)")
    lines.append("")
    lines.append("Per-bench-iteration heap allocations (delta from baseline). "
                 "Most symmetric primitives report 0 because they use "
                 "stack-only buffers with `NO_WOLFSSL_SMALL_STACK`. The "
                 "cumulative concurrent peak (across the whole bench) is "
                 "in the `peak heap` summary below.")
    lines.append("")
    header = ["Board"]
    for _, label in MEM_KEYS:
        for cfg in CONFIGS:
            header.append(f"{label} ({cfg})")
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "|".join(["---"] * len(header)) + "|")
    for board in BOARD_ORDER:
        if board not in data:
            continue
        row = [f"`{board}`"]
        for key, _ in MEM_KEYS:
            for cfg in CONFIGS:
                v = data[board].get(cfg, {}).get(key + "_heap")
                row.append(fmt_cell(v))
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def render_peak_heap(data: dict) -> str:
    lines = []
    lines.append("### Cumulative peak concurrent heap (bytes)")
    lines.append("")
    lines.append("Maximum concurrent heap occupancy across the entire bench "
                 "run, reported by `WOLFSSL_TRACK_MEMORY` at "
                 "`wolfCrypt_Cleanup`. This is the worst-case RAM budget "
                 "for a single-threaded application that exercises every "
                 "algorithm; it is platform-independent and dominated by "
                 "the largest SP-math intermediate (RSA / ECC / ML-KEM).")
    lines.append("")
    header = ["Board"] + list(CONFIGS)
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "|".join(["---"] * len(header)) + "|")
    for board in BOARD_ORDER:
        if board not in data:
            continue
        row = [f"`{board}`"]
        for cfg in CONFIGS:
            v = data[board].get(cfg, {}).get("peak_bytes")
            row.append(fmt_cell(v))
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def render_caveats(data: dict) -> str:
    lines = []
    captured = []
    missing = []
    for board in BOARD_ORDER:
        if board not in data:
            missing.append(board)
            continue
        for cfg in CONFIGS:
            metrics = data[board].get(cfg, {})
            if not metrics:
                missing.append(f"{board}:{cfg}")
            else:
                captured.append(f"{board}:{cfg}")
    if not missing:
        return ""
    lines.append("### Known caveats")
    lines.append("")
    lines.append("Cells missing from the tables above (log file absent, run "
                 "skipped, or no bench lines parsed):")
    lines.append("")
    for m in missing:
        lines.append(f"- `{m}`")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log-dir", default=str(LOG_DIR_DEFAULT),
                    help="path to bench_logs/ (default: alongside this script)")
    args = ap.parse_args()
    log_dir = Path(args.log_dir)
    if not log_dir.exists():
        print(f"error: log dir {log_dir} does not exist", file=sys.stderr)
        return 2
    data = collect(log_dir)
    if not data:
        print(f"error: no parseable logs in {log_dir}", file=sys.stderr)
        return 2
    print(render_symm(data))
    print()
    print(render_asym(data))
    if _has_mem_data(data):
        print()
        print(render_stack(data))
        print()
        print(render_heap(data))
        print()
        print(render_peak_heap(data))
    extras = render_caveats(data)
    if extras:
        print()
        print(extras)
    return 0


if __name__ == "__main__":
    sys.exit(main())

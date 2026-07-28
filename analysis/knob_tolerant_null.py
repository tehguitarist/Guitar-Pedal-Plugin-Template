#!/usr/bin/env python3
"""Null-test each capture against the plugin, KNOB-TOLERANT.

WHY THIS EXISTS. A capture's filename/settings encode the knob position someone read off a real
pot — "9 o'clock", "0.30", whatever the parser gives you (`captures.parse_capture()`). That reading
is a BEST ESTIMATE, not ground truth: a physical knob has no digital readout, so the true position
easily differs from the label by a few percent. Nulling the plugin against the capture at the exact
labelled setting therefore conflates two different things whenever the null is shallower than
expected:
  (a) the MODEL is wrong (taper, topology, component value), or
  (b) the LABEL is wrong (the knob was actually turned a hair off where it says it was),
and (a) is a real bug while (b) is capture-protocol noise that no amount of DSP tweaking will fix.

WHAT THIS SCRIPT DOES. For each capture: render at the labelled ("nominal") settings and null-test
it (frac-sample align + optimal-gain subtract, via analyze.py), THEN run a small local search
(coordinate descent) over the continuous knob parameters within +/- `--tolerance` of the labelled
value, re-rendering at each trial, and report the DEEPEST null found. The gap between "nominal" and
"best" tells you which case you're in:
  - big gap   -> the label was probably off; judge the model by the BEST null, and don't chase the
                 nominal-label residual as if it were a real accuracy bug.
  - small gap -> the label was accurate; the nominal null IS the honest model-accuracy number.
This mirrors (and now automates) the coordinate-descent diagnostic described in
docs/validation-and-capture.md's null-test section — read that first for the full rationale
(including why Volume is excluded: it's pure level, already removed by the gain match).

WHAT IT DOES NOT DO. It never changes which revision/model is "correct" — it only tells you how
much of a shallow null to blame on capture-label uncertainty vs a real shape/timbre mismatch. It is
NOT a replacement for `gap_audit.py`'s FR/THD grading (§1b, curve-shape) — those measure the SAME
kind of "isolated point vs real signal" question for a different signal (the null residual here,
the FR/THD curve there).

Cost: one OfflineRender per trial. Coordinate descent over K knobs with a 4-point search per knob
per round costs ~4*K renders per round; keep `--tolerance`/`--knobs`/`--limit` tight for a quick
pass, widen for a thorough one.

Usage (from repo root):
  python3.11 analysis/knob_tolerant_null.py                       # every capture, default 5% tol
  python3.11 analysis/knob_tolerant_null.py --tolerance 0.08      # allow a wider knob mismatch
  python3.11 analysis/knob_tolerant_null.py --rev V1E             # one revision
  python3.11 analysis/knob_tolerant_null.py --knobs drive,tone    # only tweak these settings
  python3.11 analysis/knob_tolerant_null.py --segment sweep_drv_-12   # null on a driven sweep instead
"""
import argparse
import os
import sys
import tempfile

import numpy as np

import analyze as A
import captures as C

DEFAULT_BIN = C.RENDER_BIN
DEFAULT_SEGMENT = "sweep_clean"
DEFAULT_TOLERANCE = 0.05  # +/- 5% of the 0..1 knob range — a plausible "read the pot wrong" margin
DEFAULT_EXCLUDE = ("volume", "vol", "level", "output", "output_trim", "input_trim")
GRID_FRACS = (-1.0, -0.5, 0.5, 1.0)  # offsets from center, as a multiple of the current step size
ROUNDS = 2
IMPROVEMENT_FLAG_DB = 1.0  # a knob-tolerant null deeper than this vs nominal -> label likely off


def tweakable_knobs(parsed, only, exclude):
    if only:
        return [k for k in only if k in parsed]
    return sorted(k for k, v in parsed.items()
                  if isinstance(v, (int, float)) and not isinstance(v, bool)
                  and k.lower() not in exclude)


def render_to_array(binpath, settings, os_factor, tmpdir):
    args = C.render_args(settings)
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False, dir=tmpdir) as tmp:
        out_path = tmp.name
    try:
        cmd = [binpath, A.ORIG, out_path, "--os", str(os_factor)] + args
        import subprocess
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write(f"    ! render failed for {settings}: "
                              f"{r.stderr.strip() or r.stdout.strip()}\n")
            return None
        return A.load(out_path)
    finally:
        if os.path.exists(out_path):
            os.unlink(out_path)


def make_evaluator(binpath, os_factor, orig, cap_seg, segment, tmpdir, cache):
    def evaluate(settings):
        key = tuple(sorted((k, round(v, 6) if isinstance(v, float) else v)
                            for k, v in settings.items()))
        if key in cache:
            return cache[key]
        ren = render_to_array(binpath, settings, os_factor, tmpdir)
        if ren is None:
            cache[key] = None
            return None
        ren_al, _ = A.align(ren, orig)
        ren_seg = A.seg_of(ren_al, segment)
        ren_seg_aligned = A.frac_align(ren_seg, cap_seg)
        null_db, _ = A.null_depth(cap_seg, ren_seg_aligned)
        cache[key] = null_db
        return null_db

    return evaluate


def coordinate_descent(base_settings, knobs, tolerance, evaluate):
    """Minimize null_db (more negative = deeper/better) by nudging each knob within +/- tolerance
    of its labelled value. Interacting (coupled) controls are handled by re-visiting every knob
    every round rather than optimizing each once — see circuit.md on coupled networks."""
    best_settings = dict(base_settings)
    best_null = evaluate(best_settings)
    if best_null is None:
        return best_settings, None, 0

    n_evals = 1
    step = tolerance
    for _ in range(ROUNDS):
        improved = False
        for k in knobs:
            center = best_settings[k]
            for frac in GRID_FRACS:
                trial_v = min(1.0, max(0.0, center + frac * step))
                if trial_v == best_settings[k]:
                    continue
                trial = dict(best_settings)
                trial[k] = trial_v
                n = evaluate(trial)
                n_evals += 1
                if n is not None and n < best_null:
                    best_null, best_settings = n, trial
                    improved = True
        if not improved:
            step /= 2.0
            if step < tolerance / 8:
                break
    return best_settings, best_null, n_evals


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--os", type=int, default=4)
    ap.add_argument("--segment", default=DEFAULT_SEGMENT,
                    help="capture segment to null against (default: %(default)s)")
    ap.add_argument("--tolerance", type=float, default=DEFAULT_TOLERANCE,
                    help="+/- knob search radius in 0..1 units (default: %(default)s)")
    ap.add_argument("--knobs", default=None,
                    help="comma-separated settings keys to tweak (default: auto-detect all "
                         "numeric, non-excluded keys)")
    ap.add_argument("--exclude", default=",".join(DEFAULT_EXCLUDE),
                    help="comma-separated settings keys to never tweak (default: %(default)s)")
    ap.add_argument("--rev", default=None)
    ap.add_argument("--limit", type=int, default=None, help="only process the first N captures")
    a = ap.parse_args()

    if not os.path.exists(a.bin):
        sys.exit(f"OfflineRender not found at {a.bin} — check RENDER_BIN in captures.py or set --bin")
    if not os.path.exists(A.ORIG):
        sys.exit(f"Reference not found at {A.ORIG} — run analysis/gen_test_signal.py first")

    only = [k.strip() for k in a.knobs.split(",")] if a.knobs else None
    exclude = {k.strip().lower() for k in a.exclude.split(",") if k.strip()}

    orig = A.load(A.ORIG)
    caps = C.find_captures()
    if a.rev:
        caps = [(p, parsed) for p, parsed in caps if parsed.get("rev") == a.rev]
    if a.limit:
        caps = caps[:a.limit]
    if not caps:
        sys.exit("no captures matched")

    print(f"# knob-tolerant null test: {len(caps)} captures | segment={a.segment} | "
          f"tolerance=+/-{a.tolerance} | OS={a.os}x")
    print("# 'nominal' = null at the labelled knob settings; 'best' = deepest null found within "
          "tolerance.\n# A big nominal->best gap means the capture's label is probably a few "
          "percent off the true knob position,\n# not a plugin accuracy bug — see "
          "docs/validation-and-capture.md.\n")

    with tempfile.TemporaryDirectory(prefix="knob_tol_null_") as tmpdir:
        for path, parsed in caps:
            cap = C.load_capture(path)
            if not A.is_full_length(cap, orig):
                print(f"[{parsed.get('rev', '?')}] {os.path.basename(path)}  SKIP (truncated)")
                continue
            cap_al, _ = A.align(cap, orig)
            cap_seg = A.seg_of(cap_al, a.segment)

            knobs = tweakable_knobs(parsed, only, exclude)
            if not knobs:
                print(f"[{parsed.get('rev', '?')}] {os.path.basename(path)}  "
                      "no tweakable knobs found (check --knobs/--exclude)")
                continue

            cache = {}
            evaluate = make_evaluator(a.bin, a.os, orig, cap_seg, a.segment, tmpdir, cache)
            nominal_null = evaluate(dict(parsed))
            if nominal_null is None:
                print(f"[{parsed.get('rev', '?')}] {os.path.basename(path)}  render FAILED")
                continue

            best_settings, best_null, n_evals = coordinate_descent(
                parsed, knobs, a.tolerance, evaluate)

            label = f"[{parsed.get('rev', '?')}] {os.path.basename(path)}"
            print(label)
            print(f"  nominal null: {nominal_null:6.2f} dB   "
                  + ", ".join(f"{k}={parsed[k]:.3f}" for k in knobs))
            if best_null is not None and best_null < nominal_null - 1e-6:
                delta = best_null - nominal_null
                flag = " <== KNOB TOLERANCE (label likely off)" if -delta > IMPROVEMENT_FLAG_DB else ""
                print(f"  best null:    {best_null:6.2f} dB   "
                      + ", ".join(f"{k}={best_settings[k]:.3f}" for k in knobs)
                      + f"   (Δ={delta:+.2f} dB, {n_evals} renders){flag}")
            else:
                print(f"  best null:    (no improvement within +/-{a.tolerance} — "
                      f"nominal setting is accurate, {n_evals} renders)")
            print()


if __name__ == "__main__":
    main()

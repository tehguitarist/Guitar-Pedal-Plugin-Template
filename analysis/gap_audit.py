#!/usr/bin/env python3.11
"""Read `reports/comprehensive_data.json` and grade every FR band + THD point against the
project's acceptance thresholds, so the Phase-10 gap backlog can be checked for coverage
without eyeballing a 260k-line JSON.

Grading (user-set, `--huge`/`--target` to override):
  |Δ| > 3.0 dB   HUGE    a real problem, must be tracked as an issue
  |Δ| > 1.5 dB   target  worth improving
  |Δ| <= 1.0 dB  good

Run `comprehensive_report.py` first to (re)generate the JSON. That report gain-matches the
plugin to each capture before differencing (the captures are NAM output — level is normalized
away, see README "What the captures are"), so every Δ here is SHAPE, not loudness.

Usage (from repo root):
  python3.11 analysis/gap_audit.py                  # per-revision aggregate (start here)
  python3.11 analysis/gap_audit.py --mode detail    # per-capture, per-band deviations
  python3.11 analysis/gap_audit.py --mode thd       # THD(f) curves, plugin vs pedal
  python3.11 analysis/gap_audit.py --mode shape     # curve-level tilt + contiguous-run check
  python3.11 analysis/gap_audit.py --rev V1E        # one revision

WHY `--mode shape` EXISTS (read this before trusting a clean `--mode summary` table). Point-by-point
grading can't see two real failure modes:
  - A systematic TILT (bass light, treble heavy or vice versa) where every individual band is
    "good" or "target" in isolation, but the trend end-to-end is an audible, real EQ error. No
    single point ever crosses HUGE, so `--mode summary` reports a clean sheet.
  - A single-band "HUGE" flag that's actually the front edge of a CONTIGUOUS run every sibling
    capture agrees on (i.e. probably a real, physically-motivated curve feature — see
    `capture_outlier_scan.py`'s docstring for a case this nearly cost real data) versus a true
    ISOLATED one-band spike (much more likely noise or a narrow, genuine local anomaly).
`--mode shape` fits a straight-line trend (dB/octave) through each revision/level's per-band mean
Δ and reports the run-structure of flagged bands, specifically so a "PASS" from `--mode summary`
doesn't get treated as "the FR curve is right" without also checking its shape. See
`docs/validation-and-capture.md` §1b.
"""
import argparse
import json
from collections import defaultdict
from pathlib import Path

REPORT = Path(__file__).parent / "reports" / "comprehensive_data.json"

# Bands outside this range are excluded from the STRICT (1.5 dB) grading pass but are still
# checked against a relaxed 3 dB threshold (see EXTREME_LO/EXTREME_HI in report_audit.py).
# At the bottom: below ~25 Hz the excitation sweep carries little energy, so the derived FR
# and THD band readings have degraded SNR — but for BASS applications this range contains the
# low-E fundamental (~41 Hz) and should be verified with a longer, bass-weighted sweep.
# At the top: above ~12.9 kHz the measurement SNR degrades (the source signal's energy drops
# naturally); a dB delta here is noisier, not meaningless. Saturation products reach well into
# this octave and the FR shape here affects perceived brightness, so the relaxed 3 dB pass
# still applies — the band is measured, just graded with wider tolerance.
GRADE_LOW_HZ = 25.0
GRADE_HIGH_HZ = 12900.0

DEFAULT_HUGE = 3.0
DEFAULT_TARGET = 1.5


def grade(delta, huge, target):
    a = abs(delta)
    if a > huge:
        return "HUGE"
    if a > target:
        return "target"
    return "good"


def in_grade_range(f):
    return GRADE_LOW_HZ <= f <= GRADE_HIGH_HZ


def load(rev_filter):
    d = json.loads(REPORT.read_text())
    caps = d["captures"]
    if rev_filter:
        caps = [c for c in caps if c["rev"] == rev_filter]
    return d, caps


def mode_summary(d, caps, huge, target):
    """Per-revision, per-band mean/spread across captures. The SPREAD matters as much as the
    mean: a large spread at one band means the error is setting-dependent (a taper/drive-tracking
    problem), while a consistent mean with small spread is a fixed shape error (a component value).
    """
    bands = d["meta"]["bands"]
    revs = sorted({c["rev"] for c in caps})

    for label, want_clean in (("CLEAN sweep", True), ("DRIVEN sweeps (-18/-12/-6)", False)):
        print("=" * 92)
        print(f"PER-REVISION BAND SUMMARY — mean Δ dB (plugin−pedal, gain-matched) — {label}")
        print("=" * 92)
        for rev in revs:
            acc = defaultdict(list)
            for c in caps:
                if c["rev"] != rev:
                    continue
                for level, fr in c["fr"].items():
                    if (level == "sweep_clean") != want_clean:
                        continue
                    for i, f in enumerate(bands):
                        if in_grade_range(f):
                            acc[f].append(fr["plugin_db"][i] - fr["pedal_db"][i])
            if not acc:
                continue
            print(f"\n--- {rev} ---")
            for f in bands:
                vals = acc.get(f)
                if not vals:
                    continue
                mean = sum(vals) / len(vals)
                spread = max(vals) - min(vals) if len(vals) > 1 else 0.0
                g = grade(mean, huge, target)
                mark = {"HUGE": " <== HUGE", "target": " <- target", "good": ""}[g]
                print(f"  {f:8.1f}Hz  mean={mean:+6.2f}  spread={spread:5.2f}  n={len(vals):2d}{mark}")
        print()


def mode_detail(d, caps, huge, target):
    """Per-capture, per-sweep-level band deviations — use to trace one setting's behaviour."""
    bands = d["meta"]["bands"]
    print("=" * 92)
    print("PER-CAPTURE FR DEVIATIONS (plugin−pedal, gain-matched)")
    print("=" * 92)
    for c in caps:
        for level, fr in c["fr"].items():
            hugev, targetv = [], []
            for i, f in enumerate(bands):
                if not in_grade_range(f):
                    continue
                delta = fr["plugin_db"][i] - fr["pedal_db"][i]
                g = grade(delta, huge, target)
                if g == "HUGE":
                    hugev.append((f, delta))
                elif g == "target":
                    targetv.append((f, delta))
            if hugev or targetv:
                gdb = fr.get("gain_db_applied")
                gtxt = f" [gain-matched {gdb:+.2f}dB]" if gdb is not None else ""
                print(f"\n[{c['rev']}] {c['id']} ({level}){gtxt}")
                if hugev:
                    print("  HUGE:   " + ", ".join(f"{f:.0f}Hz:{v:+.1f}" for f, v in hugev))
                if targetv:
                    print("  target: " + ", ".join(f"{f:.0f}Hz:{v:+.1f}" for f, v in targetv))


DEFAULT_NOTICE = 1.0  # a smaller-than-"target" per-band delta still counts toward a contiguous run


def _fit_trend(points):
    """Least-squares line through (log2(f), delta) pairs. Returns (slope_db_per_oct, intercept,
    residual_rms) with no numpy dependency. slope is dB per octave; residual_rms is the spread
    left AFTER removing the trend (what's left over once the tilt itself is accounted for)."""
    n = len(points)
    if n < 2:
        return 0.0, (points[0][1] if points else 0.0), 0.0
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    mean_x = sum(xs) / n
    mean_y = sum(ys) / n
    sxx = sum((x - mean_x) ** 2 for x in xs)
    sxy = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
    slope = sxy / sxx if sxx > 0 else 0.0
    intercept = mean_y - slope * mean_x
    resid = [y - (slope * x + intercept) for x, y in points]
    resid_rms = (sum(r * r for r in resid) / n) ** 0.5
    return slope, intercept, resid_rms


def _runs(flagged):
    """Group a sorted list of (freq, delta, sign) into contiguous same-sign runs. Adjacent here
    means adjacent in the BAND LIST (no gap), not just nearby in frequency — a run with a clean
    band in the middle is two runs, not one."""
    runs, current = [], []
    for item in flagged:
        if current and item[2] == current[-1][2]:
            current.append(item)
        else:
            if current:
                runs.append(current)
            current = [item]
    if current:
        runs.append(current)
    return runs


def mode_shape(d, caps, huge, target):
    """Curve-level check, complementing the per-point grade above (§1b of
    docs/validation-and-capture.md): (1) fit a trend line (dB/octave) through each band-mean
    curve to catch a systematic tilt that no single point is large enough to flag on its own;
    (2) group flagged bands into contiguous runs vs isolated spikes, since a multi-band run is
    much more likely to be a real curve feature (correct OR a real shape error) than an isolated
    single-band deviation, which is more likely measurement noise or a genuinely narrow anomaly.
    """
    import math

    bands = d["meta"]["bands"]
    revs = sorted({c["rev"] for c in caps})
    TILT_NOTE_DB_PER_OCT = 0.15  # ~0.15 dB/oct over a 20Hz-12kHz span is ~1.4 dB end-to-end

    for label, want_clean in (("CLEAN sweep", True), ("DRIVEN sweeps (-18/-12/-6)", False)):
        print("=" * 92)
        print(f"CURVE SHAPE — trend + contiguous-run check — {label}")
        print("=" * 92)
        for rev in revs:
            acc = defaultdict(list)
            for c in caps:
                if c["rev"] != rev:
                    continue
                for level, fr in c["fr"].items():
                    if (level == "sweep_clean") != want_clean:
                        continue
                    for i, f in enumerate(bands):
                        if in_grade_range(f):
                            acc[f].append(fr["plugin_db"][i] - fr["pedal_db"][i])
            means = [(f, sum(v) / len(v)) for f, v in acc.items() if v]
            means.sort()
            if len(means) < 2:
                continue

            trend_points = [(math.log2(f), m) for f, m in means]
            slope, intercept, resid_rms = _fit_trend(trend_points)
            octaves = math.log2(means[-1][0] / means[0][0])
            end_to_end = slope * octaves
            tilt_flag = " <== TILT" if abs(slope) > TILT_NOTE_DB_PER_OCT else ""

            print(f"\n--- {rev} ---")
            print(f"  trend: {slope:+.3f} dB/oct  ({end_to_end:+.2f} dB end-to-end over "
                  f"{octaves:.1f} oct, {means[0][0]:.0f}-{means[-1][0]:.0f} Hz)"
                  f"  residual(after detrend) rms={resid_rms:.2f} dB{tilt_flag}")
            if tilt_flag:
                print("    every band above may individually grade 'good', but this end-to-end "
                      "trend is a real, audible EQ tilt — see docs/validation-and-capture.md §1b")

            flagged = [(f, m, "+" if m > 0 else "-") for f, m in means if abs(m) > DEFAULT_NOTICE]
            for run in _runs(flagged):
                lo, hi = run[0][0], run[-1][0]
                kind = "CONTIGUOUS RUN" if len(run) >= 3 else "isolated"
                worst = max(run, key=lambda r: abs(r[1]))
                g = grade(worst[1], huge, target)
                print(f"  {kind:15s} {lo:8.1f}-{hi:8.1f}Hz  ({len(run)} bands, "
                      f"worst={worst[1]:+.2f}dB@{worst[0]:.0f}Hz, grade={g})")
                if kind == "isolated" and g == "HUGE":
                    print("    single-band HUGE with clean neighbors — check this against sibling "
                          "captures (capture_outlier_scan.py) before assuming it's a plugin bug")
        print()


def mode_thd(d, caps, huge, target):
    """THD(f) plugin vs pedal on the Farina-swept bands. THD is a RATIO, so it is immune to the
    capture level-normalization — these numbers are trustworthy without gain-matching."""
    bands = d["meta"]["bands"]
    sources = d["meta"]["thd_band_sources"]
    print("=" * 92)
    print("THD(f) — plugin vs pedal (Farina-swept + discrete-tone bands)")
    print("=" * 92)
    for c in caps:
        for level, thd in c.get("thd", {}).items():
            rows = []
            for i, f in enumerate(bands):
                p, g = thd["plugin_pct"][i], thd["pedal_pct"][i]
                if p is None or g is None:
                    continue
                rows.append((f, p, g, sources[i]))
            if not rows:
                continue
            print(f"\n[{c['rev']}] {c['id']} ({level})")
            for f, p, g, src in rows:
                # Flag where the two disagree by more than 2x AND the gap is audibly large.
                flag = "  <-- MISMATCH" if abs(p - g) > 5 and (g > 2 * p or p > 2 * g) else ""
                print(f"  {f:8.1f}Hz  plugin={p:6.2f}%  pedal={g:6.2f}%  ({src}){flag}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mode", choices=("summary", "detail", "thd", "shape"), default="summary")
    ap.add_argument("--rev", choices=("V1E", "V1L", "V2"), default=None)
    ap.add_argument("--huge", type=float, default=DEFAULT_HUGE)
    ap.add_argument("--target", type=float, default=DEFAULT_TARGET)
    a = ap.parse_args()

    if not REPORT.exists():
        raise SystemExit(f"{REPORT} not found — run: python3.11 analysis/comprehensive_report.py")

    d, caps = load(a.rev)
    if not caps:
        raise SystemExit(f"no captures matched --rev {a.rev}")
    print(f"# source: {REPORT}  generated={d['meta']['generated']}  OS={d['meta']['os_factor']}x")
    print(f"# grading: HUGE>|{a.huge}|dB  target>|{a.target}|dB  graded band {GRADE_LOW_HZ:.0f}-{GRADE_HIGH_HZ:.0f}Hz")
    print(f"# captures: {len(caps)}\n")

    {"summary": mode_summary, "detail": mode_detail, "thd": mode_thd,
     "shape": mode_shape}[a.mode](d, caps, a.huge, a.target)


if __name__ == "__main__":
    main()

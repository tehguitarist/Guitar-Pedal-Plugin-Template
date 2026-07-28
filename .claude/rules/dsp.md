# DSP Rules (generic, circuit-modelled pedal)

> Calibration & gain-staging lessons live in `docs/calibration-and-gain-staging.md` — read that
> too. This file covers WDF modelling, oversampling, ADAA, and the chowdsp_wdf gotchas.

## WDF Implementation

- Use **`chowdsp_wdf`** (header-only, C++17) for all circuit modelling.
- Use the **compile-time API** (`chowdsp::wdft` namespace), not the runtime `chowdsp::wdf` one —
  the compiler inlines all adaptors for near-zero call overhead. Only fall back to runtime API if a
  topology genuinely cannot be fixed at compile time (rare).
- **Use `double` for all WDF types.** `float` causes audible errors in diode Newton-Raphson at
  audio rates. Template every `ResistorT`, `CapacitorT`, `DiodePairT`, `DiodeT`, `RtypeAdaptor`,
  etc. on `double`.
- Op-amp stages: use the **ideal op-amp** model (`IdealVoltageSourceT` as the tree root driving an
  R-type adaptor, or `IdealOpAmpT` if present in your chowdsp_wdf version — check the header).
- **R-type adaptors for any feedback topology.** Derive the scattering matrix from nodal equations.
- **Never rebuild the WDF tree at runtime** for switch changes — precompute one scattering matrix
  per topology and swap via `setSMatrixData()` at the R-type adaptor.
- VREF = signal ground throughout: model **bipolar**, no power-supply node modelling (but DO model
  the op-amp output rails as a saturation — see calibration doc §6).

### Fixed (non-runtime) circuit variants

A factory/kit modification that's permanently present on only one of several otherwise-identical
stages (e.g. one of two series gain circuits is built with one resistor changed) is **not** a
runtime switch — it's a different stage instance, chosen once at construction
(`Stage(bool variant)`), with no APVTS parameter, no atomic, and no per-block check. Don't model it
as a `setSMatrixData()` swap unless the topology itself changes shape: if the variant only changes
a resistance value and your R-type matrix is built from an `ImpedanceCalculator` that reads port
impedances live (rather than a value baked into a precomputed table), changing the resistor constant
at construction is enough — the matrix recomputes itself correctly with no second precomputed
topology to maintain. Reserve precomputed-matrix swaps (`setSMatrixData()`) for actual runtime mode
switches with genuinely different port topologies (see "Never rebuild the WDF tree" above).

### Ideal op-amp decomposition (the workhorse pattern)

For an ideal op-amp the (−) input sits at the (+) input voltage and draws no current, so a
feedback stage decomposes into two independent one-ports:
```
Gain-set leg Zg (− input -> AC ground)   : Ig = Vin / Zg
Feedback leg  (− input -> output)        : Vf = (voltage Ig develops across the feedback leg)
Vout = Vin + Vf            (non-inverting)   // gain = 1 + Zf/Zg
```
This avoids a full R-type solve for simple stages. Nonlinear elements (clipping diodes) sit in the
feedback leg and only clamp `Vf` — the op-amp holds the (−) node regardless. Confirm output
polarity with a **DC-step test** in every stage; only add a `PolarityInverterT` if the readout sign
genuinely requires it (NOT reflexively for "inverting" op-amps — verify against the schematic).

**Reconstructing a node voltage: use only PASSIVE ports, never a source port.** When an output (or
any internal node) is read by *combining* two port voltages so a shared node term cancels, every
port in that combination must be a passive element (resistor, capacitor, or R+C series) — not an
`IdealVoltageSourceT`/`ResistiveVoltageSourceT` port. A source port's incident/reflected wave is
scheduled one sample apart from the rest of the tree, so reading its voltage mixes `Vs[n]` and
`Vs[n-1]` — a spurious 2-point-average low-pass. This is easy to miss because the error *looks like*
generic bilinear-cap warping (a smoothly-drooping high end) rather than an obvious bug, and can chase
a sizeable error in a stage's peak/corner frequency before the real cause (the source-port read) is
found. If a frequency-shaping stage's measured peak/corner is off by more than the expected bilinear
warp (see "Top-octave accuracy" below), check this before reaching for a prewarp fix.

### prepareToPlay requirements (missing these = silence or wrong behaviour)

- Call `.prepare(sampleRate)` on **every** `CapacitorT` / `CapacitorAlphaT` in every stage.
- Reset the oversampler.
- Each stage exposes `prepare(double sampleRate)` chaining down to its caps; the processor calls
  them in signal-chain order. JUCE calls `prepareToPlay` on every sample-rate change, so this also
  handles SR changes between sessions.

## Nonlinear elements (clipping diodes)

```cpp
// Antiparallel pair (symmetric clip):
wdft::DiodePairT<double, decltype(next), wdft::DiodeQuality::Good, AccurateOmega> dp { next, Is, Vt, nDiodes };
// Single diode (asymmetric clip):
wdft::DiodeT<double, decltype(next), wdft::DiodeQuality::Best, AccurateOmega> d { next, Is, Vt, nDiodes };
```
- Use **explicit per-component datasheet parameters**, never generic defaults. `nDiodes` is the
  **ideality factor n** (Shockley), NOT a physical count. (1N4148: Is=2.52e-9, Vt=25.85e-3, n=1.752.)
- chowdsp diodes have no series-Rs parameter; add an explicit `ResistorT` in series if Rs is
  audibly significant (usually negligible at guitar levels).
- **Two (or more) identical diodes in series collapse to ONE diode with a scaled ideality factor.**
  For the ideal Shockley equation `V = n·Vt·ln(I/Is + 1)`, identical diodes carrying the same series
  current sum their voltages linearly in `n`: k diodes in series ≡ a single diode with the same `Is`
  and `n_eff = k × n`. So a network like "two diodes in series, mirrored by another two in series
  the other way" is electrically just **one** symmetric `DiodePairT` with `n_eff = 2n` — do not
  instantiate multiple `DiodePairT`/`DiodeT` objects for a stacked string; that models independent
  parallel paths, not a series stack, and gets both the threshold voltage and the small-signal
  behaviour wrong. Verify the simplification against the schematic's actual stack count, not assumed.

### Asymmetric clip modes & even harmonics — use a PER-POLARITY diode mismatch

**Check captures for asymmetric harmonics on EVERY saturation stage, even ones the schematic draws
as a textbook-symmetric antiparallel pair.** The schematic tells you the nominal topology, not the
component tolerance or the DC bias point the real circuit actually sits at — both of which show up
only in a captured harmonic spectrum, never in the drawing. Don't reason "the schematic shows a
matched pair, so I can skip the even-harmonic check" — run the low-frequency-tone FFT (calibration
doc §6b, "Validating clipping: harmonics, saturation, and the 'go hotter' trap") against a real
capture for every clip mode before
concluding a stage is symmetric. The rest of this section exists because that check found even
harmonics on the reference build's *nominally symmetric* positions, not just its dedicated asym mode.

`DiodePairT` is **symmetric**; `DiodeT` is **one-sided** (clips one polarity, the other runs to the
rail → strongly even-dominant). Two real-pedal facts to reproduce: (a) a dedicated "asym" switch
position is asymmetric (strong-ish even harmonics); (b) even the *nominally symmetric* positions show
measurable **even harmonics** (in the reference build ~−47..−55 dB H2 re fundamental at high drive) —
because real diodes have a forward-voltage spread between the two antiparallel devices and the
above-mid-supply VREF bias offsets the operating point. A perfectly-matched ideal model produces NONE
(even harmonics at the −140 dB floor), so it is *less* faithful than one that models the tolerance.

**The model that does both, cleanly: a MISMATCHED antiparallel pair** — the +swing diode uses
`Vt·(1+m)`, the −swing `Vt·(1−m)` (per-polarity effective thermal voltage; same `Is`). Properties:
- At `m=0` it is bit-identical to the matched `DiodePairT` (each polarity's reflection is 0 at `a=0`).
- Even harmonics scale with `m`; **odd harmonics, THD, and level are unchanged** (the mismatch is
  symmetric about the average `Vt`, so one peak grows as the other shrinks — net level preserved, even
  at large `m`; it does NOT run hot).
- **No small-signal-gain artifact:** the asymmetry acts only WHERE THE DIODES CONDUCT. At small signal
  both diodes are high-Z so each polarity reflects ≈ unity → near-zero-signal gain matches the matched
  pair exactly. Use a small `m` for the symmetric positions (tolerance) and a larger `m` for a "single
  diode" asym position (a heavily-mismatched pair approximates one-sided clipping). Calibrate each to
  the captured H2 — ideally from a **hot-reamp** capture (see below).

**Two traps this avoids.** (1) A per-polarity *RATIO* (e.g. 1 diode one way, 2 in series the other)
matches the harmonics ONLY by clipping the loose side ~4 dB louder — level then **couples** to the
asymmetry (it ran hot, nulled worse). A small *symmetric* `±m` mismatch doesn't, because it's centred.
(2) A **lateral wave-domain bias** (`b(a)=symPair(a+bias)−symPair(bias)`) also adds even harmonics at
fixed level, but it shifts the operating point at ALL levels, perturbing near-zero-signal gain by up
to ~20 % at a large bias — an unphysical low-level artifact. The per-polarity mismatch has neither
problem; prefer it. (Still: an asymmetric clip produces **signal-dependent DC** — model the output
coupling cap, a ~6 Hz DC-block highpass, or it leaks DC. Still honours the OmegaProvider, no omega4
floor.)

**Diagnosing a "high-drive THD ceiling".** If the plugin seems to under-distort at high drive, first
match the INPUT LEVEL (a hot-reamp capture is ideal) and compare a per-harmonic FFT — usually the odd
harmonics + overall THD already match and the "ceiling" was a level-calibration artifact; the only
real gap is the missing even harmonics above. Don't chase it with global EQ; it's clipping asymmetry.

### Reverse-breakdown zener-pair clips (back-to-back zener in an op-amp feedback leg)

Some pedals clip with an **antiparallel zener pair** in an op-amp feedback leg rather than forward
diodes — on each swing one device conducts forward (~Vf 0.6 V) while the other reverse-**breaks
down** at its rated Vz, so the pair clamps at an effective `Vth = Vf + Vz` (e.g. a 3.3 V zener pair
clamps around ±3.9 V). `chowdsp_wdf`'s `DiodePairT`/`DiodeT` model forward Shockley conduction only
(turn-on fixed near ~0.6 V) and cannot place a several-volt knee without an absurd, ill-scaled `Is`
— don't try to force a plain diode pair into this role by tweaking `Is`/`Vt`; it needs its own
element.

**Modelling approach — reparameterise the antiparallel-diode wave solve, don't add a new element
class.** The pair's I-V is odd-symmetric and both branches' "−1" terms cancel →
`I(V) = 2·Is·sinh(V/Vt)`, which is *exactly* the antiparallel-diode law — so the WDF reflection is
the same Werner et al. eqn-18 form `DiodePairT`'s `Good` path already uses, just with `(Is, Vt)`
reparameterised from the zener's physical knee instead of a datasheet diode:
`Vt = Vzt` (knee softness), `Is = Iref·exp(−Vth/Vzt)` (pins `I(Vth) = Iref` at the datasheet test
current). Template it on the omega provider like any other diode element — `DiodeQuality::Best`
hardcodes omega4 (see below), so build this on the `Good`-path reflection directly. Junction
capacitance goes in as a `CapacitorT` **in parallel** with the pair (two series junction caps →
about half a single device's Cd), re-discretised on `prepare()` so its corner stays
sample-rate-independent; it sets the HF rolloff downstream of the clip and is usually the dominant
cause of a "the top end is darker on this clip mode" mismatch, not the zener knee itself.

**The knee-softness trap that costs real time: do NOT set `Vzt` from the datasheet `r_dif`.**
`r_dif` (dynamic resistance) is measured deep in breakdown and is far too soft as a single-exponential
knee — it leaks meaningfully below the rated Vz, which *destroys the small-signal linear gain* and
clamps soft well under the rating. A sharp knee (roughly an order of magnitude tighter than `r_dif`
suggests) keeps the sub-knee region open and holds the clamp near the actual rated voltage — much
closer to how a real zener behaves. Ground `Vth`/`Iref` from the datasheet's Vz-at-test-current row;
treat `Vzt` as a separate, sharper fit parameter, not derived from `r_dif`.

**What you need from captures to fit this accurately (this is the part that's easy to get wrong):**
- **Multiple drive/level settings, not just one "sounds about right" capture.** The knee softness
  (`Vzt`) and junction capacitance (`Cj`) are visible in *how the harmonic spectrum changes shape*
  as level rises through the knee — a single capture at one level underdetermines both and any fit
  will overfit to that one operating point.
- **A capture set with enough independent settings to separate Cj from Vzt/Vth from any downstream
  EQ.** If every capture you have also varies a tone/level control alongside drive, a change in HF
  content can't be attributed to Cj vs. that control — fit Cj only from a **matched-pair** capture
  set (same everything, drive/level swept alone) or accept the fit is confounded and say so.
  `docs/calibration-and-gain-staging.md`'s matched-pair pattern for tapers applies just as much here.
- **Don't fit Cj (or any junction parameter) from a small, non-matched capture set and expect a
  decisive answer.** If the residual error is nearly flat across a wide parameter range (no clear
  minimum) instead of showing a sharp minimum, that's the tell the capture set can't arbitrate the
  parameter — stop and get better captures (or accept the schematic/datasheet-nominal value) rather
  than shipping whichever flat-residual value happened to score best.
- **A rising or falling harmonic-vs-level slope is a signature of MEMORY, not a static zener
  parameter — don't keep re-fitting `m`/asymmetry/knee looking for it.** If a capture set shows
  (say) H2 magnitude changing slope with drive level in a way no static asymmetry (mismatched
  knee, mismatched Vz, feedback-rail asymmetry) can reproduce — because every static asymmetry's
  harmonic contribution is flat or monotonic-with-level in one direction, and the capture shows the
  opposite or a reversal — that is evidence the real circuit has a level-dependent operating point
  (a self-bias node drooping under asymmetric draw, a decoupling cap not fully settled, etc.), which
  a memoryless per-sample zener model structurally cannot produce. Treat this as its own investigation
  (does the schematic have a self-bias/decoupling node whose droop has the right time constant and
  polarity — a paper feasibility check before touching code) rather than another turn of the
  static-parameter fitting crank. Confirm with an independent second parameter (e.g. if an `m`-style
  mismatch fit and a separate rail-asymmetry fit **both** fail to converge on the same capture set,
  that's two independent refutations of the static-parameter class, not one investigation that
  needs a third parameter to try.
- **A cheap value swap between two similar zener parts (same nominal Vz, different Cj/knee) is a
  fast way to sanity-check whether Cj is even in the right neighbourhood** — drop one part's fitted
  Cj into the other's clip stage and see if the match improves or regresses. A regression tells you
  the two stages' captures actually need different Cj values (real per-part variance), not that
  your fitting method is broken. Tag any such swap `[PROBE]` in the diff and revert it once answered
  — see the artificial-corrections guardrails in `docs/calibration-and-gain-staging.md`.

**Validate against an independent solve, not just "it sounds right":** an exact-Newton DC solve of
the same `(Is, Vt)`-reparameterised device, at several drive levels, should agree with the WDF
reflection to a fraction of a percent below the knee and tighter through it. Confirm THD rises
monotonically with drive across at least three levels spanning well below/into/above the knee — a
non-monotonic curve means the solver is struggling, not that the circuit is doing something exotic.

### Omega accuracy gotcha (do NOT use the default omega)

chowdsp's default `Omega::omega` (omega4) uses bit-trick log/exp approximations that impose a
~−35 dB distortion floor — audible on a "transparent" pedal. Supply a custom **AccurateOmega**
provider (std::log/exp + a few Newton steps solving `w + ln(w) = x`).
**Trap:** `DiodePairT`'s `DiodeQuality::Best` path HARDCODES omega4 and ignores the provider — use
**`DiodeQuality::Good`** for the pair (eqn-18; accurate once given a true omega). `DiodeT` and the
pair's `Good` path both honour the provider. Verify with an audible-band aliasing test.

### HQ / Eco mode (gating CPU-vs-accuracy features)

Don't add an HQ button reflexively — let `FeatureProfile` (`build.md`) decide. Measure each feature's
CPU cost AND accuracy delta together; gate ONLY features that are a real lever (meaningful CPU for
audible accuracy). Leave free/near-free features always-on (a toggle for them is just clutter).

- **Usually the only real lever is the omega solver:** `omega4` is markedly cheaper (the diode solve
  dominates DSP cost) but adds a ~−30..−44 dB distortion floor. So HQ on = AccurateOmega, off =
  omega4. **Implement as a RUNTIME switch**, not two template instantiations: a `bool highQ` in the
  diode class that branches the omega call per sample (predictable branch → effectively free), with a
  `setHighQuality(bool)` plumbed processor → DSP → stage → diode. Keep the omega-provider TEMPLATE
  too (defaulted) so `FeatureProfile` can still A/B at compile time. Add a `FeatureProfile` guard
  asserting HQ-off is bit-identical to the omega4 chain, so the button can't silently become a no-op.
- **Typically NOT worth gating (measure to confirm):** rail-clip ADAA (≈0 CPU for a big aliasing
  cut), oversampling the downstream linear tone stages (cheap, fixes the top octave), diode mismatch
  (≈0 CPU, it's a faithfulness feature). Oversampling factor itself is already the user's master
  quality/CPU knob — often that, plus this one omega toggle, is all you need.
- **Other levers to scope IF the profile flags them:** AccurateOmega Newton-iteration count (4→2 is a
  minor sub-lever of the above); the JUCE oversampling FIR vs a cheaper polyphase-IIR (saves up/down
  cost but is non-linear-phase — only if the FIR shows up as a real cost). Park these as notes unless
  CPU is genuinely a problem; absolute cost is often already small (one accurate instance ≈ low
  single-digit % of a core).
- UI: a lit-on / dim-off toggle in the OS/scale strip with a brief customer-facing tooltip; `hq`
  `AudioParameterBool` default true (see `architecture.md`).

## Oversampling

- Oversample for the **nonlinear stage** (the aliasing source), but let the region SPAN any
  downstream linear stages that have audible-band HF caps (tone/recovery) — see Top-octave below.
  Only leave OUT linear stages with no audible HF caps (e.g. an input ~8 Hz HP). Pattern: give the
  oversampler a per-OS-sample `postFn` overload that runs those downstream stages, and prepare them
  at the oversampled rate.
- `juce::dsp::Oversampling`; minimum 4×, prefer 8× for clipping. Expose 1×/2×/4×/8× in the UI.
- Re-discretise every oversampled stage's caps at the oversampled rate so its response is preserved.
- Glitch-free factor switching: detect a pending change via `std::atomic<int>`, and at block start
  `reset()` + `initProcessing(maxBlock)` then update the factor (one-block gap is acceptable; do
  NOT try to crossfade an OS change).
- Consider a **separate render-time OS factor**: in `processBlock`, pick the higher factor when
  `isNonRealtime()` is true (offline bounce) — see architecture.md.

## Top-octave accuracy: bilinear cap warping near Nyquist

Linear stages run at base rate, and chowdsp's trapezoidal capacitor (companion `R = 1/(2 C fs)`) is
the bilinear transform — it bends the frequency axis, so an analog corner at `f_c` lands at a
*lower* digital frequency, the error growing toward Nyquist. Symptom: the modelled top octave is
**too dark** vs the real pedal even with tone controls flat (the reference build was ~−3.8 dB at
12 kHz / 48 kHz from a ~16 kHz treble corner + a ~7 kHz feedback corner). Diagnose by rendering the
**same signal at 2× base rate** (resample in, render, resample out) — if the deficit closes and
matches the real unit, it's warping, not a modelling error.

Two fixes, a real trade-off:
- **Prewarp the HF caps** (`utils/Prewarp.h`): replace `C` with `C·θ/tan(θ)`, `θ = π·f_c/fs`, pinning
  the corner where the real circuit has it. Zero CPU, no architecture change, no added coloration —
  it just relocates corners. Exact at the pinned corner, excellent through ~12–14 kHz, slightly soft
  right at Nyquist. Recompute per-block for a cap whose corner moves with a pot. Best for low-order,
  well-separated corners. **Only prewarp BASE-RATE linear caps** — a cap inside the oversampled
  nonlinear stage is already discretised at the high rate (the oversampler fixes its warp; prewarping
  it too would over-correct). **Don't prewarp a peak/corner that sweeps with a knob across a wide
  range** (e.g. a gain-stage resonance whose peak frequency moves with a drive control) — prewarping
  pins ONE frequency, so it only matches the analog response at the knob position you pinned it to,
  and is silently wrong everywhere else on that knob's range. For a knob-dependent peak, either
  accept the warp (validate that the *gain* and DC/limiting behaviour are still correct at the base
  rate, and document the frequency warp as a known, bounded inaccuracy) or oversample that stage
  instead, which tracks the moving peak correctly at every knob position.
- **Oversample the downstream linear HF stages** (extend the nonlinear oversampling region to cover
  tone + recovery): flat to 20 kHz regardless of topology, mode-INDEPENDENT (it's a pure
  discretisation fix, so it behaves identically in every clip mode — the right answer when you need
  the top octave correct in ALL modes), and the OS factor then actually improves the top octave.
  Costs ~N× the (cheap, linear) tone/recovery CPU. Implementation: a templated `processBlock(data, n,
  postFn)` on the oversampler runs `postFn` (the downstream linear stages) per OS-sample; prepare
  those stages at `getOversampledRate()` and re-prepare on factor change. In the reference build this
  recovered ~+8 dB at 12 kHz (heavy-cut setting) and pulled 12 kHz from ~8 dB-dark to within ±2 dB of
  the real unit; at the default 4× it already ≈ the true-analog response, < 4 kHz unchanged.
  **Keep prewarp as well** — it's what fixes the top octave at the 1× (no-oversampling) setting.
  Recommended over prewarp-alone whenever the deficit is audible; prewarp-alone is the zero-CPU
  fallback. (The two are complementary, not exclusive.)

- **Low-OS top-octave restore (a cheap third option, complements both).** Even with prewarp, at LOW
  oversampling the tone caps' bilinear Nyquist zero still droops the top octave (reference: 1× ≈
  −4 dB @8k / −10 @12k / −21 @16k; 2× ≈ a quarter of that in dB; 4×/8× negligible — measure with
  `OSFidelity`). The droop is essentially POT-INDEPENDENT and scales with the OS factor, so a single
  fixed-shape high-shelf (one biquad at base rate, gain set PER OS factor, ~0 at 4×/8× so it's
  transparent at the default) recovers most of it — 1× to within ~±1 dB through 12 kHz. It can't
  invert the near-Nyquist zero (16 kHz stays down — accepted, least audible). Always-on (self-
  disables where there's no droop); makes low-OS "sound close" so high-OS only refines aliasing.

Independent of supply-voltage / rail features (those scale amplitude headroom; prewarp corrects
frequency) — the two never interact.

## ADAA (antiderivative anti-aliasing)

- ADAA is **in addition to** oversampling, not instead of it.
- Apply it where the **hardest** nonlinearity is. In the reference build the dominant aliaser was
  the **op-amp rail clip** (a hard clamp), not the soft diodes (whose fast-decaying harmonics
  oversampling already crushes). So 1st-order ADAA wrapped `railClip` (exact piecewise
  antiderivative), and diodes relied on oversampling + AccurateOmega. The chowdsp diode models also
  expose no closed-form antiderivative, so diode ADAA needs a bespoke omega-antiderivative — only
  worth it if listening reveals residual diode aliasing at low OS factors.
- 1st-order ADAA: `y = (F1(x) - F1(xPrev)) / (x - xPrev)`, with a midpoint fallback when
  `|x - xPrev|` is tiny. Update the state every sample so toggling is glitch-free.
- Reference: Esqueda et al., "Antiderivative Antialiasing in Nonlinear Wave Digital Filters",
  DAFx 2020.

## Pot tapers

- Honour the schematic's taper (audio/log vs linear). Build kits often substitute linear for cost —
  do NOT follow the kit, follow the schematic.
- See `utils/TaperUtils.h` and calibration doc §3 for the **audio-taper floor trap** on large pots.
- **The `10^(2x-2)` audio approximation is too aggressive** (only ~10% R at midpoint vs ~35-40% for
  a real audio pot) — it makes tone controls far too shallow. Prefer fitting a **power-law taper**
  (`R = Rmax * x^p`) to captures, with Rmax ≈ the schematic pot value. See calibration doc §3b. Tone
  pots inside a feedback gain-set leg are coupled to gain — re-check levels after retapering them.
- **Fit the taper SHAPE (p), and don't assume convex.** p≈1.4 (convex) is only a starting guess. A
  subtle "trim"-style tone cut can be **concave** (p<1: fast rise to a moderate R, then ~flat) — the
  reference build's treble was `~12k·x^0.4`. Tell-tale of a wrong shape (not just wrong coeff): you
  can match ONE knob position but the error flips sign at another (e.g. too bright at 9 o'clock yet
  too dark at 3 o'clock). So constrain p with **at least two** knob points across the full range.
- **Isolate a coupled control with a MATCHED-PAIR capture.** When a control only appears in captures
  alongside clipping/other controls (so the linear EQ is confounded), capture two settings that
  differ in **only that one knob**, everything else identical. The clipping/other effects are then
  identical in both and **cancel in the difference**, giving a clean differential measurement of
  that control's contribution — even from driven captures. (This rescued a treble fit that raw
  per-capture transfers couldn't, because the clean sweep wasn't actually clean at drive.)

## Signal calibration

- Anchor `kInputRef` (volts per full-scale) from a real measurement — calibration doc §1.
- Internal nominal reference: pick one (e.g. −12 dBu) and stay consistent.
- Provide input + output trims, visually distinct from the pedal controls.

## Coupled controls

- Controls sharing a network (e.g. bass + drive in one feedback web) must be modelled as a **single
  coupled WDF network**, not independent processors. Use
  `wdft::ScopedDeferImpedancePropagation` when updating several parameters at once.

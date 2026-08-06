# Modelling the parts WDF can't model natively

> Generic reference for the components a circuit-modelled pedal will hit that `chowdsp_wdf` has no
> element for — CMOS-inverter clippers, JFET gain stages, and op-amp output rails. Everything here
> is component-class knowledge, not pedal-specific: read it before writing any nonlinear stage, and
> re-read §5 before shipping one.
>
> Companion reading: `.claude/rules/dsp.md` (WDF/ADAA/oversampling rules),
> `docs/calibration-and-gain-staging.md` (level anchoring), `docs/validation-and-capture.md`
> (how to capture the reference so a fit is trustworthy).

---

## 0. Triage — what actually needs an external model

Do this table for your pedal **before** any DSP work. Most of the parts list is WDF-native and needs
nothing; usually only one or two components genuinely require you to supply a model.

| Component class | WDF-native? | What you need |
|---|---|---|
| R, C, bridged-T, Sallen-Key, Baxandall/tone networks | ✅ | `ResistorT` / `CapacitorT`, nothing external |
| Op-amps (TL07x, NE5532, JRC4558, …) | ✅ ideal op-amp + rail clamp | one datum: the output swing on your supply (§3) |
| Signal-path silicon/germanium/LED/zener clippers | ✅ `DiodeT` / `DiodePairT` | datasheet `Is`, `Vt`, ideality `n` |
| **CMOS inverter used as an amp** (CD4049UB, CD4069UB) | ❌ | a fitted VTC inside a solved feedback loop — §1 |
| **JFET / MOSFET gain stages** (J201, 2N5457, BS170) | ❌ | a fitted transconductance + shaper, or a WD 3-terminal device — §2 |
| Supply diodes, status LEDs, VREF dividers | n/a | not in the signal path — don't model |

⭐ **The triage itself is the deliverable.** Writing "only these two parts need external data" down
early is what stops a later session inventing a model for something `CapacitorT` already handles.

---

## 1. CMOS inverter clippers (CD4049UB / CD4069UB "Red Llama" class)

An **unbuffered** CMOS inverter section wired as a shunt-feedback inverting amp, self-biased to its
own transition point by the feedback resistor, clipping softly against its own supply rails. Common
in bass/high-gain overdrives. **This is the audible distortion** — model the inverter's voltage
transfer curve, not any protection diodes around it.

### Sources
- **★ DAFx-2020, "Taming the Red Llama" (Köper & Holters)** — models exactly this topology. Gives a
  **measured 9 V VTC** (the curve datasheets omit) plus a fitted two-MOSFET Shichman–Hodges model.
  Simple-model params at VDD = 9 V, per section:
  - n-ch: α ≈ 5.1021e-3, vT ≈ 1.5702 V
  - p-ch: α ≈ 8.2246e-4, |vT| ≈ 0.48476 V, λ ≈ 0.06 V⁻¹

  Their extended model makes α and vT functions of vGS. Use it as a **ground-truth curve generator**
  to fit a smooth waveshaper against. Their solver is DK-method state-space — the *model* is the
  contribution, redo the numerics in WDF.
  Local: `docs/refs/DAFx2020_Taming-the-Red-Llama_CD4049-overdrive-model.pdf`.
  Code: https://lkoeper.gitlab.io/dafx-2020-cmos-llama/ · merged into https://github.com/HSU-ANT/ACME.jl
- **TI CD4049UB datasheet SCHS046L** — local: `docs/refs/TI_CD4049UB_datasheet_SCHS046L.pdf`.
  The VTC figure is **5 V only and is a min/max tolerance envelope**, not a typical curve; the
  MOSFET I-V families are what DAFx fit. Rails: VOH ≈ VCC − 0.05, VOL ≈ 0.05. CIN ≈ 15 pF.
- ⛔ **There is no usable analogue SPICE subckt.** The widely-circulated `CD4000_v.lib` `CD4049B` and
  TI's PSpice model are **behavioural digital** macromodels — switching only, no soft clip. Using one
  for distortion gives you a hard comparator. Build the inverter from two generic MOSFETs with the
  DAFx params instead.

### Recommended model
A **static asymmetric-sigmoid VTC inside the shunt-feedback loop**, solved per sample.

```
node W (inverter input) KCL, worked in a frame where 0 = the self-bias trip point Vm:
    F(W) = G_in*(x - W) - Ic + g_fb*(VTC(W) - W) - ieq_fb  = 0
    F'(W) = -G_in + g_fb*(VTC'(W) - 1)
```

Four things about this that are easy to get wrong, each of which cost real time:

1. ⭐⭐ **Finite open-loop gain is VOICING, not a refinement.** Real unbuffered CMOS sections measure
   A₀ ≈ 20–30, not ∞. The input node's impedance is `R_fb/(1+A₀)`, which **dominates** the input
   coupling cap's high-pass RC. Modelling the input as an ideal virtual ground can put your bass
   corners a factor of 3–5 too high and the closed-loop gain 3× too hot. ⇒ the input resistor, the
   coupling cap(s), the finite-gain VTC and the feedback R∥C are **ONE coupled stage**, never
   "high-pass, then waveshaper". Fit A₀ from the bass-corner voicing plus the drive-sweep level.
2. ⭐ **Asymmetry is intrinsic and required.** The n-ch and p-ch thresholds differ by ~3× and their
   α by ~6×, so the VTC is genuinely asymmetric and generates even harmonics. Fit per-side
   saturation levels against low-frequency-tone H2/H3 — don't bolt a token mismatch onto a
   symmetric curve.
3. ⭐⭐ **Find the supply dropper.** These stages very often have a series resistor between the rail
   and the inverter's VDD pin — it is usually the ONLY IC with one, and it is the trick that makes
   the stage sound the way it does. The unbuffered inverter draws mA-scale class-A crowbar current in
   its linear region, so the clip ceiling sits **well below** the op-amp rail and sags with signal.
   The drop is **computable, not a guess** — the current and the voltage determine each other:

       VDD = V_rail − I_DD(VDD)·R_drop        (implicit — solve it)

   Solve `Id_n(Vm) = Id_p(Vm)` at the self-bias point with the DAFx MOSFET params and iterate. The
   feedback is self-limiting (crowbar current is super-quadratic in VDD), so a fixed-drop prior
   cannot express it and will be wrong. ⚠ **The answer depends on how many sections are active** —
   verify on the schematic that the spare inverter inputs are tied to a rail (correct CMOS practice,
   ~µA quiescent). If they float at mid-rail all six sections draw and the solved rail collapses by
   half. Check this at high zoom on the actual drawing; it is a one-line fact that moves the whole
   ceiling.
4. **Scale the VTC to that solved rail.** For a rail-to-rail CMOS output the per-side saturation
   levels are output *swings*, so **their sum is bounded above by VDD_eff**. That bound is one-sided
   — a sum below the rail is not a violation — but a sum far below it means your fit has quietly
   made the clipper see less signal than the circuit does (see §5's degeneracy warning).

Oversample this stage and ADAA it (§5.1) — in a chain that contains one, it is almost always the
hardest aliaser.

---

## 2. JFET gain stages (J201, 2N5457, and friends)

Typically a common-source gain stage, sometimes with a second JFET as an active load. Mildly
nonlinear, and part of the pedal's character rather than its distortion. chowdsp has no JFET element.

⚠⚠ **The part is spread ~5:1 device-to-device** (a J201's Vgs(off) is specified −0.3…−1.5 V and IDSS
0.2…1.0 mA). **Nominal SPICE will not match a specific unit.** Every amplitude parameter must be
fitted to a capture; only the R/C corners and the polarity are trustworthy in advance.

### Sources
- **Fairchild/onsemi J201 datasheet** — local: `docs/refs/Fairchild_J201_datasheet.pdf`. DC params
  and the spread that forces the fit.
- **SPICE `.MODEL` sets** (starting points, not answers):
  - Datasheet-derived: `NJF(VTO=-0.718 BETA=1.031M LAMBDA=2M IS=114.5F RD=1 RS=1 CGD=4.667P CGS=2.992P M=.2271 PB=.5 FC=.5)`
  - LTspice factory pair, useful as an A/B for the spread:
    `VTO=-0.6 BETA=1.6m LAMBDA=2.2m` and `VTO=-0.93 BETA=1.1m LAMBDA=6.8m`
  - Consensus: BETA ≈ 1.0–1.6 mA/V², LAMBDA ≈ 2m, CGD ≈ 4.7p / CGS ≈ 3p. **VTO is the disagreement
    (−0.6…−0.93 quoted, −0.3…−1.5 allowed) → fit it.**
- **★ Bernardini et al., "WD Modeling of Nonlinear 3-terminal Devices" (CSSP 2019)** — JFET drain
  current as a 3rd-order polynomial giving **explicit closed-form wave scattering**, no per-sample
  Newton for the device. The reference if you go the full-solve route.
  https://link.springer.com/article/10.1007/s00034-019-01331-7
- **DAFx-2024 MXR Phase 90** — local: `docs/refs/DAFx2024_MXR-Phase90_JFET-timevarying-resistor-WDF.pdf`.
  ⚠ Models JFETs in the **voltage-controlled-resistor** regime, not as a driven amp — a cheap-
  approximation reference, less applicable to a gain stage.
- RT-WDF (Werner, DAFx 2016) — nonlinear 3-terminal device at an R-type root; the triode case study
  is the structural analogue of a CS amp.

### Recommended model — Path B: fit the block, not the device

```
x --[input HP]--[gate divider]--[1/k(s)]--[static shaper]--*(-gm)--> i_drain
     (C_in,R_g)                 (Cs/Rs)
```

Memoryless-ADAA-friendly, cheap, per-unit calibratable. Path A (a full coupled 2-device WDF solve at
an R-type root) is the fallback — escalate only if an A/B shows the static model misses audible
dynamics, which is unlikely if a harder clipper sits downstream.

⭐⭐⭐ **THE STRUCTURAL TRAP, AND IT IS WORTH ~20 dB: A DEGENERATED CS STAGE IS A CURRENT SOURCE,
NOT A VOLTAGE SOURCE.** The obvious model — high-pass → "HF-lift shelf" → gain → shaper, feeding the
next stage as an ideal source — is wrong. For source degeneration `Zs = Rs ∥ Cs`:

```
    k(s)    = 1 + gm·Zs(s)      degeneration factor: 1+gm·Rs at DC → 1 at HF
    Gm(s)   = gm / k(s)         transconductance RISES with frequency
    Rout(s) = ro · k(s)         drain output resistance FALLS with frequency
    ⇒ open-circuit gain Gm·Rout = gm·ro — FLAT, independent of the degeneration
```

So the source-bypass cap's "HF lift" is **not an unconditional gain lift**. It appears only to the
extent the stage is *loaded*, and the load (a tone ladder) typically has an input impedance that
falls across the same band, cancelling most of it. Applying the shelf unconditionally **and** driving
the next stage from an ideal source double-counts the boost.

✅ **Fix:** output the drain **Norton current**, and stamp the stage's output impedance
`Zout(s) = [ro·k(s)] ∥ R_load_active` into the *next* stage's nodal matrix. The shelf survives only
as the shared `k(s)` shaping Gm and Rout in opposite directions — exactly as the device does.

Two consequences worth keeping:
- Drive the shaper with the **effective vgs** (real gate volts, order |Vp| ≈ 0.3–1.5 V), so the
  shaper's knee is in physical units rather than an arbitrary post-gain scale. Small-signal current
  is then exactly `−gm·vgs`, so `gm` alone sets the gain and the shaper only adds curvature (slope at
  0 is exactly 1). ⚠ Any shaper parameters fitted before such a restructure are **meaningless
  afterwards** — refit, don't rescale.
- This is a Wiener–Hammerstein approximation: true degeneration is nonlinear feedback
  (`vgs = vg − i_d·Zs`, an implicit solve). Linearising the degeneration and putting the
  nonlinearity on vgs is a modelling *choice* — say so in the header, so a later session knows it is
  a known approximation rather than an oversight.

### The shaper shape — three findings that cost sessions each

**(a) ⭐⭐ A tanh cannot produce an even-dominant stage.** If the captured low-drive character is
even-dominant (e.g. H2 ≈ −36 dB against H3 ≈ −59 dB, a ~23 dB even/odd separation), a tanh
**structurally cannot reach it**: tanh is an odd map, so its cubic term forces H3 whenever it makes
H2. That is the fingerprint of a **square-law** transfer, which is exactly what a JFET has. Use a
linear-core-plus-even-bump shape:

```
    g(w) = T(w) + (a·s²/2)·tanh²(w/s)
```

The bump is **exactly even**, so it contributes zero odd content at any drive; all of g's odd part
is in the core `T`. Its small-signal expansion is `a·w²/2`, so `a` is the square-law quadratic
coefficient (≈ 1/Vov near the origin), and it has a clean antiderivative:
`∫ = (a·s²/2)(w − s·tanh(w/s))`.

⚠ Read "the odd part is T" carefully: it stops meaning "the odd part is linear" the moment T's
limiting is asymmetric.

**(b) ⭐⭐⭐ CHECK THE SIGN OF THE CUBIC BEFORE CHOOSING A LIMITER.** A compressive sigmoid's H3 is
intrinsically ~180° out of phase with a downstream clipper's H3 at the chain output. If the reference
device's third harmonic is **in phase** with the clipper's, no compressive shape — however you tune
its knee hardness — can reproduce it; hardness only rescales the magnitude and walks both drive
settings through a shared anti-phase null. The lever is the **sign of the cubic term**, and it needs
an *expansive-then-bounded* core:

```
    T(w) = w·(1 + c·w²) / (1 + (w/L)²)^(3/2),   c = β + 3/(2L²)
    ⇒ T(w) = w + β·w³ + O(w⁵)     — β IS the cubic coefficient, by construction
```

β > 0 gives expansive (in-phase) H3; β = 0 is cubic-free; β < 0 recovers the compressive regime as a
special case of the same family. T stays bounded (`T → ±(βL³ + 1.5L)`), T(0) = 0 and T′(0) = 1
**exactly** on both sides — so `gm` remains the small-signal transconductance and every linear
oracle/FR test is untouched. Per-side L gives the asymmetry; keep β shared, because asymmetry is an
*even* lever and belongs on the per-side limits, not duplicated onto the odd one.

⛔ **Do not build this by composing a pre-warp with a sigmoid** (`T(w) = Sigmoid(w + βw³)`). Composing
two nonlinear maps has no elementary antiderivative in general — the k=2 case reduces to an
elliptic-type integral — which **destroys closed-form ADAA**. The rational form above is a single
elementary map engineered so its own series is expansive and its own tail saturates, and it keeps a
real antiderivative:

```
    F(w) = c·L³·√(L²+w²) + (c·L⁵ − L³)/√(L²+w²),   G(w) = F(w) − F(0)
```

**(c) ⚠⚠ MONOTONICITY: DERIVE THE BOUND, THEN SCAN THE REAL COMBINED FUNCTION.** A bound derived for
one sub-term is **not** a bound on the sum. A commonly-quoted limit for the even bump alone
(`|a|·s < 2.598`) is 7 % loose once the core's own negative curvature near the knee subtracts from
the bump's slope — the combined map folds back earlier than the sub-term algebra predicts. Quote a
sub-term bound as an **upper bound on the admissible region, never as the region**, and scan the
shipped function on a fine grid before shipping any parameter triple. A candidate rejected as
"worse-scoring" may actually have been non-monotone.

For the expansive core the analytic bound is clean and worth having:

```
    T′(w) = L³·(L² + w²·(3L²β + 2.5)) / (√(L²+w²)·(L²+w²)²)
```

denominator always positive, so for β ≥ 0 the bracket is a sum of two strictly positive terms and
T′ > 0 **unconditionally**. Fold-back exists only for β < −2.5/(3L²), outside the useful regime.
Gate it numerically anyway.

---

## 3. Op-amp output rails (minor, but don't skip it)

WDF-native: ideal op-amp for the feedback solve, plus a **separate output saturation clamp**. The
only external datum is the output swing on your actual supply.

- TL07x and most classic parts are **not rail-to-rail** — they swing to within ~1.2–1.5 V of each
  rail, asymmetrically (worse toward the negative rail). On a single 9 V supply after a series
  Schottky (~8.6 V rail) that is roughly [1.2, 7.8] V.
- **Confirm by capturing a stage driven into its rails** rather than trusting the nominal. No SPICE
  model is needed — it is just a clamp.
- ⭐ Clamps deserve ADAA more than soft nonlinearities do: a hard clamp is often the *dominant*
  aliaser in a chain, and its piecewise antiderivative is exact and free.
- ⛔ **Don't put a rail clamp on something that isn't an op-amp output.** A JFET drain or a CMOS
  inverter output limits by its own device physics; that limiting belongs in the device's shaper, and
  bolting a second op-amp-style clamp on top double-limits it. Conversely, check that *something*
  limits: an unbounded shaper with rail clamping disabled can leave nothing at all between the input
  jack and the first hard clipper.

---

## 4. Capturing the reference so these can be fitted

Audio-only (in → out) captures are enough for all three classes. See
`docs/validation-and-capture.md` for the protocol; the parts that matter for *nonlinear* fitting:

- **Bypass pass first** — the level anchor. Fix interface gain for the whole session and never touch
  it. 48 kHz / 32-bit float, no interface clipping.
- **A drive sweep** (4–5 points) is what fits the clipper: swept-THD(f) for the amount, low-frequency
  tones for the H2/H3/H4 asymmetry.
- **At minimum drive the harder clipper barely clips**, so the OD path's residual nonlinearity there
  is approximately the JFET stage alone. That is how you separate two cascaded nonlinearities from
  in→out audio with no probing.
- **A pre/post switch comparison** (e.g. the same signal with the drive path engaged vs bypassed, EQ
  flat in both) isolates the OD path's own *linear* shaping without any probing.
- ⚠⚠ **Watch for a capture-interface ceiling.** If several unrelated max-boost captures all pin at
  the *same* peak value with flat-topped samples, that is one interface headroom limit, not N
  coincidences. Drop the send and re-capture the affected takes — and **tag the re-captured files in
  their filename** so a lower-gain take can never silently look like a normal one.
- ⚠ Calibrate a gain-session delta from a **clean-path** anchor pair, never a driven one: a
  compressed path turns a −12 dB send change into a much smaller output change and is useless as a
  linear reference.

### The electrical-values gap
Without probing you cannot measure DC bias or exact clip voltages. Take them as **nominal** from the
datasheets, then **calibrate the effective ceiling to the bypass + drive captures** — the model's
clip level is fitted to the measured onset-of-clipping and output level, which is what is audible,
recovering the missing electrical values indirectly from audio.

---

## 5. Cross-cutting rules for any non-WDF-native stage

### 5.1 ADAA
- ⭐⭐ **ADAA1 applies to a nonlinearity buried inside an implicit solve.** "The stage has memory" is
  **not** a disqualification — the derivation needs only (a) a memoryless map and (b) an argument
  that is ~linear between samples. It says nothing about that argument being the *stage's* input. A
  VTC solved on an internal node is a perfectly ordinary memoryless map of that node.
- Substitute the averaged value **inside** the residual, not just at the output. Where KCL makes the
  internal node a linear combination of the input and the shaper's output, every harmonic at that
  node arrives via the output, so substituting inside antialiases the node too — free. Substituting
  only at the output leaves the solve and the emitted sample disagreeing about the branch current and
  corrupts the companion-cap state.
- The averaged map stays monotone if the original is, so root uniqueness survives — but a closed-form
  `|f| ≤ sat` bracket may not. Bracket off the **slope** instead.
- ⛔ **Do not split the map to dodge the 2-point-average cost.** Averaging only the nonlinear residue
  and keeping the linear term pointwise evaluates two halves of ONE map **half a sample apart**:
  algebraically that injects a first difference with gain `a₀/2`, which reaches the **full loop gain
  at Nyquist**. Measured, it ran the stage's fundamental +13.4 dB hot and made the alias floor 14.4 dB
  *worse* than plain ADAA — whose own cost was 0.01 dB. Gate ADAA off, or accept the rolloff.
- ⛔ **Do not substitute quadrature for a missing antiderivative.** Measure the argument's per-sample
  step against the map's own knee width: if the step exceeds the knee on a large fraction of samples
  (typical at realtime oversampling factors), fixed quadrature nodes land in saturation on both sides
  and miss the feature entirely. Re-anchor the shape parameter to a value whose primitive *is*
  elementary, or build a Chebyshev/table primitive.
- ⭐⭐ **Gate ADAA by oversampling factor and measure both ends.** ADAA1 is a first-order
  approximation with its own residual; its benefit shrinks as OS rises and can go **negative** where
  oversampling has already taken the floor low. Read the *worst-tone* column, not just the median,
  and express the gate as a `<=` threshold on the factor — the benefit is not monotone in rate, so
  don't interpolate it.
- ⚠ Make the gate a settable knob, not a hardcoded `if` — otherwise the gate's own validation
  measures the gate instead of the mechanism.

### 5.2 The implicit solve
- ⭐⭐⭐ **Use the textbook safeguarded algorithm verbatim (`rtsafe`), not a hand-rolled guard.** A
  safeguarded iteration needs BOTH a containment condition and a **progress** condition, and the
  second is exactly the part that looks redundant:
  - containment alone with a **strict** range test fires at the *root* (at convergence F rounds to 0,
    the Newton step is 0, the candidate lands *on* an endpoint) and bisects away from the answer it
    had already found — worse than no guard at every operating point;
  - containment alone **non-strict** permits a 2-cycle (Newton at `a` proposes exactly `b` and at `b`
    proposes exactly `a`), measured cycling 16 iterations with the step still at half the bracket;
  - range test **+** sufficient decrease (`|2f| > |dw_old·f′|`, "Newton is not at least halving") is
    exact everywhere at every rate.
- ⚠ **A strictly monotone residual gives root UNIQUENESS, not Newton's global convergence.**
  Monotonicity makes a bracket valid and says nothing about whether Newton reaches the root. A
  sigmoid is nearly flat in saturation, so a step taken from out there overshoots to the far side —
  which is the whole defect.
- ⭐⭐⭐ **An iteration budget is a claim about the STIMULUS, not just the sample rate.** Warm-started
  solvers are made easy by smooth low-frequency probes: a synthetic amplitude sweep can report
  "converged" while the real chain — a mid-band tone through the actual upstream filters — is
  unconverged on a few percent of samples with large residuals, at *every* oversampling factor.
  Sweep amplitude to find the mechanism; **quote the in-chain number**, measured with the real
  upstream chain attached.
- ⭐⭐ **To measure solver accuracy, re-solve from the shipped arm's own state each sample and record
  the one-step error.** Running two independent instances of a stateful nonlinear chain and
  differencing the outputs measures **trajectory divergence**, not solver accuracy: a trapezoidal
  companion-cap recursion is lossless, so an injected 1e-15 never decays and a high-gain map
  amplifies it. Two solvers both accurate to 4e-16 per sample were measured producing trajectories
  0.7 V apart. Both quantities are real — the divergence is what reaches the listener — but only the
  one-step figure can size a *solver*.

### 5.3 Fitting discipline
- ⛔⛔ **Watch for the "make the nonlinearity see less" degeneracy.** A monotone objective with no
  interior minimum is not a fit. Any parameter that scales what the shaper sees (an input reference,
  a coupling cap, a ceiling) can improve almost any aggregate error by simply turning the stage down.
  **Require the objective to push back from both sides**, and treat a "best value = delete the
  component" result as a refutation of the lever, not a result.
- ⚠ **Check a fitted amplitude against its physical ceiling, and check what that ceiling costs.** If
  the fitted per-side saturation sums to a small fraction of the solved supply rail, the fit has made
  the stage see less signal than the circuit does. That is worth flagging as a known soft-low rather
  than papering over downstream (widening a protection-clamp window to stop it firing is treating the
  symptom — the clamp window should be anchored on physics and the fit is what is off).
- ⭐⭐⭐ **When a parameter carries two meanings and a fit consumes one of them, every derived
  quantity that used the *other* meaning is now wrong — and nothing in the fit's validation will say
  so.** A saturation level that starts as "output swing toward the rail" is a legitimate basis for a
  protection-clamp window; the day a re-fit turns it into a fitted *knee scale*, the window silently
  follows it into the region the node actually occupies. Prefer a fix that makes the coupling
  **unrepresentable**: derive such windows from a separate physical constant that no fit can reach.
- ⭐⭐ **Anchor a fitted exponent/shape parameter on a value with a special case.** If the stage has a
  closed-form fast path or an elementary antiderivative at a particular value, name it as the
  preferred value in the fit's own comment and break accuracy ties toward it. A fitter reports the
  argmin, not the set of points indistinguishable from it, so "is the winner distinguishable from the
  special value?" is a question nobody asks unless it is written down. In one measured case the
  difference was **indistinguishable on accuracy and ~2× on total plugin CPU** — and a value off the
  anchor silently disabled ADAA entirely, with no error and no log line.
- **Expose the shaper and its antiderivative publicly** so the per-stage test can validate the
  *shipped* map directly (monotonicity by finite difference, `F′ == g`) rather than re-implementing
  it and testing a replica.
- ⚠ **A per-stage test that runs NOMINAL constants cannot certify a claim about the SHIPPED build.**
  Sort every assertion into structure-invariant (nominal is fine, and is the point) versus
  amplitude-dependent (must run the shipped fit), and run both arms for the second kind.
- **Confirm polarity with a DC-step test per stage, and end-to-end.** A common-source stage inverts;
  a CMOS inverter inverts. Where a clean path rejoins a processed one, per-stage tests can all pass
  while the *aggregate* sign at the summing node is wrong — see `dsp.md`'s dry/wet alignment section.

---

## Local reference files (`docs/refs/`)
- `DAFx2020_Taming-the-Red-Llama_CD4049-overdrive-model.pdf` — CMOS-inverter overdrive model + params.
- `TI_CD4049UB_datasheet_SCHS046L.pdf` — VTC envelope (5 V), MOSFET I-V families, rails.
- `Fairchild_J201_datasheet.pdf` — J201 DC params and the part spread.
- `DAFx2024_MXR-Phase90_JFET-timevarying-resistor-WDF.pdf` — JFET-in-WDF, VCR regime.
- Bernardini 3-terminal WDF paper — fetch from Springer/ResearchGate if going the full-solve route.

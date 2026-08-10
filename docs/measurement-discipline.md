# Measurement discipline — the traps in measuring a model against a reference

> Generic reference for the analysis side of circuit modelling: building instruments, reading
> aggregates, running fits, and screening candidate mechanisms. Every entry below is a failure mode
> that has inverted a conclusion **already written down as fact** — usually silently, and usually in
> the flattering direction.
>
> This is a rules file, not a reading list. Skim it once at the start; re-read the relevant section
> before you (a) build a new instrument, (b) trust an aggregate, or (c) ship a constant.
>
> Companion reading: `docs/validation-and-capture.md` (how to capture and what to measure),
> `docs/nonlinear-component-modeling.md` §5.3 (fitting discipline for nonlinear stages).

---

## 1. Before you trust an instrument

- **Verify the baseline reproduces BEFORE ranking anything.** Every tool that ranks candidates must
  first re-score the shipped point against its own recorded value and REFUSE to print if it moved. A
  baseline that has silently moved makes every comparison a fiction.
- **Verify the CONSTANT, not the prose.** A handover saying "SHIPPED" is a claim about a file; it is
  one `grep` to check. A value can be documented as shipped for a whole session while the source
  still holds the old one.
- **Verify the BASELINE, not its LABEL.** A row labelled "no change" that is quietly fitting a free
  broadband gain makes every "improvement over baseline" measure the wrong thing.
- **Verify the PREMISE, not the previous framing of it.** A stale premise is the most expensive kind,
  because it selects the whole next workplan. Re-measuring usually takes one command. Four corollaries,
  each paid for:
  - **A backlog line can outlive its own dissolution indefinitely, because the label travels and the
    refutation does not.** Before opening an item, grep the logs for its own NAME — a dissolution is
    normally recorded where the work happened, not where the item is listed. Then fix the line.
  - **When a premise is refuted, withdraw every verdict that rests on it in the same session.**
    Re-scoping the code that used a premise is not the same as re-scoping the conclusions it produced,
    and a summary table is exactly where conclusions outlive their support.
  - **A refutation has to land where the thing is CHOSEN, not only where it is ANALYSED.** After
    refuting an option, grep its name and fix every site that *offers* it: enum comments, defaults
    tables, CLI help, docstrings listing valid values. Keep the refuted option selectable — but say so
    at the option.
  - **"Everything is refuted" is a claim about the candidates that were NAMED, not about the space.**
    The mechanical audit: for every screen, diff the parameters its mechanism function *accepts*
    against the ones it actually *swept*. The difference is the unscreened set.
- **A passing test bounds only the region it ran in.** "The test asserts it" is a claim about one
  operating point. When a header cites a test as proof of an *always* statement, read the test's own
  conditions first.
- **A per-stage test that runs NOMINAL constants cannot certify a claim about the SHIPPED build.**
  Sort every assertion into structure-invariant (nominal is fine, and is the point) versus
  amplitude-dependent (must run the shipped fit), and run both arms for the second kind. The mirror
  error is as bad: a test that binds a claim about a *parameter* to whatever happens to be shipped
  breaks the day shipping changes, and fails against correct code.
- **An instrument defect is a hypothesis, and it is usually cheaper to test than to fix.** An argument
  for why an instrument *must* be wrong is not a measurement of it being wrong — synthesise the
  contaminated case first, and only build the replacement if the old read actually fails.
- **A failing gate is a hypothesis about the model *or* about the guard.** A bisection tells you when
  a statistic started failing, never whether the statistic was valid. Before attributing a failure to
  the thing that changed, re-derive what the guard assumes and check the physics still requires it —
  a guard going red right after a real change is maximally convincing precisely because the change
  supplies a ready mechanism.
- **When a guard's premise is wrong, DELETE the quantity — do not loosen the bar.** The two are
  indistinguishable from the failing number alone and completely different in consequence. **If the
  rebuilt test is harder than the one it replaces and still passes, it is a correction; if it is
  easier, it is a concession.**

### Known answers

- **A known answer that starts at its own answer is a fixed point, not a test.** For any iterative
  recovery, include a start far from the truth (bracketing values, random inits) and require them all
  to land in the same place. "It reproduced the reference" is evidence only if it could have failed.
- **A known answer must NAME its condition**, taken from the source it is reproducing. Choosing the
  condition by best agreement measures agreement, not the instrument.
- **A known answer that an earlier sub-gate already guarantees is a restatement, not a check.** Before
  writing one, list what the gate has already established and confirm the new test is not implied by
  it. Then run the mutation that should break it — a vacuous check looks exactly like a strong one.
- ⭐⭐ **List what both sides of a known answer share as INPUT — that shared input is precisely what
  the check cannot validate.** Comparing your implementation against a trusted one at the *same*
  element values validates the topology and is structurally blind to the value set. It needs its own
  divergence guard (assert the two value sets actually differ, and name them). A guard like this
  belongs at **every site that consumes the value set**, not only in the tool that first noticed the
  trap — grep for the accessor, not for the guard, since a shared helper multiplies one defect across
  every caller.
- ⭐⭐⭐ **The topology hands you free known answers if you ask what it FORBIDS.** A post-EQ
  attenuation-only divider into a unity buffer *cannot* have frequency structure — measuring that
  structure and finding it flat to 0.0002 dB certifies the reconstruction, the band mapping, the
  sweep handling and the provenance correction at once, for one subtraction. Look down the chain for
  a stage whose physics forbids something, and measure that.
- ⭐⭐⭐ **When the quantity is a CHANGE in a linear functional, everything that does not vary cancels
  EXACTLY.** Before building a perturbation screen, ask whether your statistic is a *difference* and
  whether the operator producing it is *linear* in what the fixed parts contribute. If both, the fixed
  blocks are not "small", they are zero — the screen may need no render at all. **Assert it** (add a
  deliberately wild fixed block, require the change to stay below 1e-9) rather than arguing it.

### Mutation-testing guards

- **Mutation-test every guard.** A guard that reads the wrong key, returns `None` on every real input
  and falls through to a "cannot verify" branch is a warning that reads as diligence while checking
  nothing.
- **Score guard IDENTITY, not just a non-zero exit.** A crash exits non-zero too. Match each failure
  line's own tag against the guard the mutation was aimed at — otherwise a mutation that dies before
  reaching the guard reads as a clean success. And a gate should **refuse** where it would otherwise
  crash: a stack trace hands the next reader a symptom instead of a reason.
- ⭐⭐⭐ **A runner that scores `rc != 0` cannot test a COMPUTED VERDICT at all** — and a well-built
  gate deliberately puts its most important statements in computed verdicts rather than exit codes.
  Add an expected **exit code** *plus* a **string the output must contain**: an arm with `expect_rc=0`
  breaks the data behind a verdict and requires the gate to print the *opposite* verdict. The third
  outcome is `NARRATED` — passed, but never printed the required line — which is how a conclusion
  that has quietly become hard-coded gets caught.
- **Suspect the mutation before the guard.** Common vacuous shapes, all of which read as `GUARD DEAD`:
  mutating a threshold that is nowhere near the data; `if False:` (which *disables* a guard rather
  than firing it); patching a module constant at runtime when workers re-import the module fresh;
  perturbing an invariance with something that does not actually break the property it rests on (a
  different *linear* functional does not break additivity — you need a nonlinear one).
- **The mutant must LIVE where the tool lives** (so sibling imports resolve) **and RUN from where the
  tool runs** (so data paths resolve) — two different requirements, and satisfying one is the natural
  way to break the other. Always run an unmutated CONTROL first; if it does not pass, no failure below
  is attributable to the mutation.
- **Give the mutant a PID-unique path**, and **redirect its output artefacts**. A faithful copy of a
  tool inherits that tool's side effects — the last arm's deliberately-falsified report is otherwise
  left on disk wearing the real gate's filename. Redirect the control run too, and make the redirect
  REFUSE if its pattern does not apply.
- **A cardinality is a useless discriminator when two classes can be the same size.** Assert on
  membership (a sorted, machine-checkable list) and on a NAME that must cross between groups.
- **Read what the failure SAYS, not just that it fired.** A message that could not be true ("count
  moved (16 vs 16)") is a defect in the test even when the gate under test is sound.
- **Concurrency bugs in test scaffolding read as gate defects** — which is the one place they cost
  double, because the arm's wrong answer looks like the gate narrating. Pass perturbations as
  **arguments**, never module-level flags shared across a thread pool.

---

## 2. Thresholds and bars

- ⭐⭐ **A threshold you guessed is not a guard.** Measure the distribution, place the bar in its gap,
  and **assert the separation** rather than a count. A generous bar feels conservative and is not — it
  is a different arbitrary number that fails in the flattering direction.
- **If the population is NOT bimodal, no threshold is defensible — and "say so" is not enough.** Make
  the bar an **axis rather than a decision**: re-measure the headline at each bar, print the table,
  and emit a computed `BAR-SENSITIVE — quote it with its bar and n` verdict. That makes the
  *direction* visibly bar-independent (the finding) while the *size* is not (the caveat), and lets a
  downstream consumer be checked for whether it even touches the sensitive quantity.
- ⭐⭐⭐ **A verdict that flips on ~1 % of its own bar is not a verdict — and the fix is not a better
  bar.** It is noticing that one verdict was carrying two questions. Split them: *is it real?* → an
  exact sign test or a permutation test, neither needing a bar; *is it big enough to matter?* → a
  swept bar, or an **r²**, which is a SHARE and needs no threshold to read. ⚠ Replacing an invented
  bar with a *conventional* one the data sits on top of (p = 0.052) is the same mistake in a lab coat
  — check whether the branches even disagree about anything consequential.
- **"The biggest gap in the sorted values" is not a bimodality test** — it is satisfied by any
  population with one big step, including a smooth continuum. Take the bar from a quantity measured
  **independently of the thing being classified** (a take-to-take floor says what "the same thing
  twice" looks like), and report anything within 3× of it as not robustly classified.
- **A threshold below your storage precision cannot pass** — work out the precision of the pipe the
  data came through (file format, sample format, round-trips) and set the bar a decade above it. A bar
  tighter than the storage is not strict, it is broken.
- **A tolerance a correct implementation cannot meet is also a broken test.** Derive the bar from the
  quantity's own **scaling law**, not as a round number, and print the ratio-to-bound. Both failure
  directions are live and feel completely different: too-tight fails alarmingly on correct code,
  too-loose passes silently on wrong code.
- **A dispersion statistic may RANK reliability; only a resolution or an independently-measured floor
  may GATE it.** A full range (`max/min − 1`) admits no error bar and cannot be converted into a
  significance in either direction. The legitimate floor is the measurement's own resolution —
  imported from the tool that defines it, not transcribed.

---

## 3. Aggregates, membership and range

- ⚠⚠ **`aggregate-moved-check-membership-first`.** An aggregate that fails to reproduce has usually
  gained or lost rows, not changed value. **Never quote a total without its count.** This is the most
  frequently-recurring entry in this file.
  - **Adding VALID data can close a gate row with no model change**, if the new rows sit at a quieter
    operating point and dilute the pool. The check is one command: re-grade the new report restricted
    to the captures the old one had, and require byte-identity. The durable fix is to **print the
    pool's composition along its dominant axis**, so a weight shift can never read as progress.
  - **It also fires inside an EPOCH comparison**, where conditions are identical by construction — a
    quality *bar* downstream of the change is not a condition, and anything that moves the graded
    quantity moves who passes the bar. Difference two epochs only on cells admitted in **both**, and
    print the pooled column beside it.
  - **A contaminated membership can leave the aggregate exactly right and only show up one level
    down.** "The headline still reproduces" is not a membership check — **assert** the membership
    (exact counts, named exclusions, one row per condition).
  - **A "cost of change X" estimate is itself a membership-dependent aggregate.** Re-measure it
    against the CURRENT baseline at the moment the decision is taken.
- ⭐⭐⭐ **A conclusion ABOUT FREQUENCY cannot be drawn from a statistic that averages over frequency**
  — and when it is, it writes the next session's workplan. Before quoting a pooled residual as
  evidence about *where* a defect lives, plot it. The same applies to any axis: a pooled statistic
  cannot answer a question about its own axis.
- ⭐⭐⭐ **An endpoint pair is not a ladder.** When a designed ladder exists, print every rung and test
  **monotonicity** before calling anything a dose-response; quote the WORST rung beside the endpoint
  one. Agreement at both ends is exactly what a crossing or a reversal looks like from outside — and a
  span over a *non-monotone* excursion is not a dose-response at all.
- ⭐⭐ **A wash-out / delta / span cannot say which end moved.** Whenever a difference is the headline,
  print its two operands per condition. If the two sides put their dependence at opposite ends, say so
  — that is a structural finding a difference cannot express.
- **Split a pooled cell before naming it a defect.** A median hides the very structure worth chasing;
  "it collapses" often means "it collapses in 3 of 5 switch settings".
- ⭐⭐⭐ **Unsigned aggregates have no sign, and a summary will eventually supply one.** An `abs()` in a
  headline is a one-way door: print the signed term beside every unsigned one, or the next reader
  invents a direction and it is 50 % likely to select the whole workplan wrong.
- ⭐⭐⭐ **Any quantity whose sign comes from a library has an arbitrary sign until you PIN it** —
  eigenvectors, SVD/QR factors, FFT phase, PCA loadings, fitted regression columns. Canonicalise at
  the source and assert a **signed** known answer (`decompose(+3 dB)` must return `+3`), not merely a
  same-signed one. The tell is *inconsistency between terms of one basis*. ⚠ And note what such a fix
  does and does not touch: every `abs()`-taking quantity is invariant, so "a sign was wrong" and "the
  grade was wrong" are different claims.
- ⭐⭐⭐ **Before asking which way a defect points, check whether it points one way at all.** Print the
  statistic over the two axes the device is actually driven on; if it crosses zero, the deliverable is
  the **surface** and a mechanism that explains a *slope*, not an offset. A constant cannot move a
  surface that changes sign, whichever sign it carries.
- **Difference statistics hide common-mode.** A ratio reading "94 % of target" while every component
  is 10 dB low is the standard shape. Print absolutes beside every difference.
- **A mean of square roots does not decompose into shares.** The pooled *mean square* does. Quote the
  statistic the share was computed on, print both, and check the decomposition recombines.
- ⭐⭐⭐ **Pool vs exclude is a false choice — SPLIT.** The test is "does pooling flip the verdict on
  the population the bar was written for?" Excluding loses data, pooling hides the defect, splitting
  does neither. **Keep the original bar on both halves** — inventing a new threshold for the new row
  is how a split quietly becomes a concession. Expect it to cost a row; a gate getting more accurate
  usually looks like a gate getting worse.
- **Report the CONDITIONAL read, not the marginal.** A marginal over one control is confounded by
  every other until you condition on the dominant one — and surviving conditioning is still not
  enough: cross-tabulate the survivors.
- ⭐⭐ **Condition on the physical quantity, not on the knob that usually sets it.** Fixing one control
  does not remove a confound that a second control also moves; it reparameterises it onto the next
  axis. Compute the quantity from the stage and bin on THAT — and if the data cannot break the
  collinearity, refuse the verdict rather than reporting the confounded number.
- **A ratio can move because its denominator moved.** Print both columns and read which one changed.
- **`ratio-statistics-need-a-denominator-guard`.** A dB number computed from something at the
  numerical floor is not a measurement: searches kill the denominator, gates manufacture failures by
  differencing floor-level numerators.
- **`defective-rows-must-not-vote`.** A constant fitted over rows containing a known unfixed defect
  lands on a compensating error. And a defective row is excluded **by name** — no combination of
  setting predicates will do it, because what is wrong with it is not in its settings. Assert the
  exclusion matched something.
- ⭐⭐⭐ **Resolve membership from SETTINGS, then ASSERT it — never from a filename substring.** A
  substring is a guess about a naming convention, and naming conventions are not versioned; it does
  not fail when written, it fails when someone adds data. Exact counts, named exclusions, one vote per
  condition.
- ⭐⭐ **Three outcomes, not two: "never had data" and "lost data" must not share a branch.** A side
  with zero readings may be a genuine physical outcome; a side with *some* of its expected readings is
  a **malformed** result. Make partial a REFUSAL (a validity failure), not an exclusion.
- **A percentage share is only defined when the components share a sign.** If two legs move opposite
  ways, state the signed magnitudes and say so.
- ⭐⭐⭐ **A pooled statistic can be RIGHT for the shipping question and WRONG for the mechanism
  question — and it is the same number.** If what ships is one value per cell, a mean over the other
  axis is exactly what would ship. For a claim about the two quantities as *measurements*, the axis
  has to stay intact. Ask which question the number is being quoted for, every time. The diagnostic:
  **print both operands of any before/after ratio** — agreeing on the "after" and disagreeing on the
  "before" localises the cancellation in one line.
- **An aggregate's RANGE can be the problem, not its membership.** A self-validation quoted over a
  narrower range than it was run on will omit exactly the bands where it failed.
- **A range boundary justified only by a COMMENT is an untested exclusion**, and the excluded band is
  as likely to be the worst as the quietest. Measure before excluding — and expect widening to cost.
- **Check `n` before reading a trend.** A smooth, physical-looking curve can rest on one value.
- **One capture is not the population**, and **a constant measured from one pair is a constant
  measured from one pair** — the pair it came from is the least likely to contradict it. When a
  calibration constant has exactly one source, the first replicate is worth more than any amount of
  downstream analysis; and if a *finding* was also derived from that source, re-check it too.

---

## 4. Gates, controls and verdicts

- **`computed-verdicts-not-narrated`.** A conclusion hard-coded into a tool's output outlives the
  condition it described and prints above a table contradicting it. Derive every verdict from the
  data, and make it state the opposite when the data says so.
- ⭐⭐⭐ **A classifier's verdict must be a comparison against the TARGET, not a property of the
  candidate.** If you can delete the target from the code and the classification still runs, it is
  narration. ⚠ A **normalisation** can delete the target while the target is still visibly in the code
  — dividing each side by its own *signed* value at a reference point destroys exactly the sign
  information the screen exists to see. Normalise by the magnitude.
- **A gate that produces no data must FAIL, not fall through to its else-branch.** An empty result
  passes checks silently and narrates a verdict over nothing. A sentinel is not a measurement — check
  the span before quoting.
- ⭐⭐ **Hard-exit on the gate's own VALIDITY, never on how the physics comes out.** Ask of every
  `exit`: "if this fires, have I learned something about my instrument, or about the device?" Only the
  first belongs there. An outcome gets a **computed verdict** and execution continues — otherwise a
  finding silently suppresses every measurement below it, and the tempting repair (raise the bar)
  deletes the finding outright. For a solve-for-the-required-value gate, split the two failures:
  unreadable endpoints are a validity failure and exit; **no root between them is a computed verdict
  scoring zero** — "no setting reaches the target in DIRECTION" is a *stronger* refutation than any
  size argument, and throwing it away is the flattering-direction bug.
- **A `nan` does not trip a threshold**, so one bad value can disable a whole gate branch while
  everything prints fine. Test `isfinite` explicitly and first; every comparison against `nan` is
  False, so a poisoned statistic fails **open**.
- **Sweep a threshold over a range that actually binds, and assert that it binds.** Print the
  surviving count beside every row and exit if the count never changes. ⚠ Unless the graded quantity
  is identically zero — then the constancy IS the finding. Distinguish by measurement, not prose.
- **Gate the property the CONCLUSION rests on**, not an absolute accuracy the statistic does not have.
  Monotonicity and round-trip recovery are usually what a downstream solve needs; asserting absolute
  accuracy fails correct code, and a guard that demands more than the conclusion needs is a bug.
- **A control must be established present AT THE CONDITIONS THE GATE READS**, not merely somewhere.
  Pick it from what a prior gate resolved on that exact condition class, name any rejected candidate
  *and its reason* so the exclusion is pre-registered, and keep printing the rejected one's values.
  ⚠ The tempting repair — loosening the presence bar — deletes the only thing making a negative result
  mean anything.
- ⭐⭐ **A negative result wants a SYNTHETIC arm, because it is the only check that can fail in both
  directions.** A broken estimator, an empty membership, a misplaced window and a genuinely featureless
  curve all print the same thing. Inject a feature of KNOWN size and sweep the size, **including
  zero** — zero is the arm's own mutation control. Gate the arm on the quantity's own *law*, not a
  round number.
- **A control measured on the quantity the instrument ANCHORS on cannot fail.** Check what an
  instrument normalises on before believing a "free" verdict; re-score per component.
- ⭐⭐ **A residual that is ~0 by single-point calibration is not evidence, on any baseline.** A term
  the model was fitted to cannot certify the model. What it *can* do is cross-check two independent
  derivations of the calibration — and that is all it may be quoted for.
- **`self-selecting-scores`.** A scan that scores candidates on "the points that fit" lets the worst
  one win by shrinking its own scoring set. Freeze the band/row set at the baseline.
- **A unifying hypothesis read off the worst-N rows is `self-selecting-scores` at its most seductive**,
  because the subset was printed for an honest reason. The worst-N table sizes a cost; it cannot infer
  a mechanism. Compute the statistic on the FULL population before believing a pattern seen in a tail.
- **`gate-domain-must-cover-candidate-reach`.** A score computed over a narrow band prefers a candidate
  whose cost lands outside it — it measures the benefit and is blind to the cost *by construction*.
- **A measurement CONDITION needs its own gate.** "Present in all positions" and "level-independent"
  are satisfied just as well by a shared *render-condition* error as by a shared circuit one. Derive
  render args from the shared module; never type them.
- ⭐ **Before adding a mechanism class, count how many instances of it the model already has — and
  read what they do.** One `grep` plus one stored report is stronger than any calculation, because it
  is the shipped build measured rather than a model of it.
- **A defect found is not a defect PRICED, and a defect that is real is not a defect that is
  SUFFICIENT.** When a gate finds a defect while hunting a regression, make it measure whether that
  defect is big enough to BE the regression. And when you fix a shared input, **measure the
  consequence** — re-run at both value sets and diff the stored reports rather than asserting the
  difference is small.

---

## 5. Fits, searches and degeneracy

- ⛔⛔ **A monotone objective with no interior minimum is a degeneracy, not a fit.** Any parameter that
  scales what a nonlinearity sees can improve almost any aggregate by turning the stage down. Require
  the objective to push back from BOTH sides, and treat "the best value is to delete the component" as
  a refutation of the lever.
- **`bound-resting-means-unidentified`** — the outside bound is the missing equation. ⚠ But that is a
  **hypothesis**, not a measurement: pin the parameter and re-fit everything else before widening
  anything. A rail *caused* by the box shows a lower cost outside; a coincident optimum shows a higher
  one, and the two are indistinguishable from the fitted point alone. ⚠ Also distinguish a genuine
  fence from a **symmetry point** (a folded parameter's endpoint is not bound-resting).
- ⭐ **A 1-D slice through a railed parameter is the least trustworthy slice there is** — the other
  coordinates were chosen *while* it was railed. Run both the slice and the re-optimised profile; let
  only the second carry a verdict.
- ⭐⭐⭐ **When the error is in *x* and not in *y*, least-squares over all points is the wrong
  likelihood.** Uncertain-y → weighted least squares. Uncertain-x with a few exact points →
  **constrain through the exact ones** and let the estimates choose only the remaining shape;
  inverse-variance weighting still lets a big enough pile of estimates outvote the one point that is
  known. Identify the exact points from something INDEPENDENT of the fit, and check they are genuinely
  independent observations (two files agreeing perfectly may be one file twice).
- ⭐⭐ **A one-signed residual diagnoses an inadequate model FAMILY; the rms does not.** Adding
  parameters always lowers rms. But measurement noise has no preferred sign, so a one-signed residual
  means the family is wrong — **print the residual's mean beside its rms, and when they disagree,
  believe the mean.** Justify a richer family on properties the objective did *not* ask for (physical
  monotonicity, convexity, a textbook range), not on its rms.
- ⭐⭐ **If every term of an objective is relative, nothing in it sees absolute level — and a fit will
  spend that blindness to the last dB.** Before optimising, list what the objective CANNOT see and
  check the search cannot reach it. A documented blind spot in a *diagnostic* becomes a degeneracy the
  moment a search is pointed at it.
- ⭐⭐ **A reachability result from a blind objective is a measurement of what the objective was willing
  to spend.** A fit does not decline to pay in a currency it cannot count. Corollary: when a newly-added
  term makes a previously-"reached" requirement unreachable, that is the earlier result being
  corrected, not the new term being too strict.
- **Imposed checks cannot corroborate.** Constraining a fit to satisfy its own independent check makes
  "it passes" circular. Free it again from that basin.
- ⭐⭐ **A fitted nuisance parameter is not a measurement.** Look for a configuration where it is the
  ONLY thing varying — an exact-zero endpoint of a mixing network usually exists and turns a
  two-parameter fit into a subtraction. When the fitted and measured values disagree by an order of
  magnitude, they may be *different quantities*: say which is which rather than picking.
- ⭐⭐⭐ **A parameter obtained by INVERTING a law absorbs that law's own residual**, so it is
  systematically wrong by exactly the amount the law misses — and it looks like a measurement of the
  device. Write the assumption beside the number, and treat measuring the parameter independently as
  the way to *size the law's residual*. The two disagreeing is the expected outcome, and their ratio
  is itself the result.
- **Measure both operands of a ratio on the same object.** "The model's curvature" can silently be two
  different things (a rendered model and a closed-form cascade). Re-measure numerator and denominator
  with one estimator on one object and report both.
- ⭐⭐ **When a fit will not close, free the nuisance parameter as a CONTROL** — it separates "my
  measurement of it is wrong" from "the model is wrong", which have opposite next steps. The control
  needs its own control: freeing it must not break the known answer, or it is too loose to arbitrate.
- ⭐⭐ **A recovered quantity that MUST be invariant and isn't is a refutation of the model form.** An
  absolute residual is always arguable; an invariance is not. Find something the parameter is
  *forbidden* to depend on and recover it separately at several values of that thing.
- ⭐⭐ **Score the candidate you will actually EMIT.** A two-stage fit whose second stage is *argued*
  harmless must print the harm — such a perturbation is usually candidate-dependent and therefore
  reorders the field, so ranking on stage-1 statistics ranks something you do not ship.
- ⭐ **Rank FEASIBILITY first — "printed" is not "scored".** A candidate that cannot be rendered is not
  a worse candidate, it is not a candidate. And expect that fixing one blind spot in a ranking key
  moves the selection into the next one: re-derive the whole key, not the term you came for.
- **Quantise a ranking key to the resolution of what it ranks** — but to compare **candidates** only,
  never to compare a measurement with its own reference. Re-using a rounding key for a
  reproduce-or-improve question turns a difference far below the term's floor into a verdict whenever
  it straddles a bin edge. **A rounding boundary is not a finding.**
- **A candidate's own search must not be seeded from the incumbent's parameterisation.** When a
  constant's meaning changes, such a seed lands outside the valid region and the "fit" silently
  returns its starting value. Multi-start from a grid over the parameter's own admissible range.
- **`search-settings-are-derived-artefacts`.** A search box justified by a measurement expires with
  it — sweep the setting rather than inheriting the choice. And when a swept setting exists, every
  mapping that reads it must read the *swept* value, not a hardcoded copy of the default.
- **Gate a search before trusting its FAILURE.** Synthesise a target you know is reachable and require
  recovery under the noise floor first.
- **A joint-fit shortfall may be ARBITRATION, not reachability** — separate the fits when the parameter
  groups do not interact.
- **`one-knob-two-jobs-is-compensating`.** A constant that trades feature A against B with no value
  good for both is propping up a different unfixed defect.
- **Don't fit jointly across levels when the element is level-invariant** — fit at one level, then
  CHECK at the others.
- ⭐⭐⭐ **A fitted constant can carry a cost the objective has no term for.** If a stage special-cases a
  value (a fast path, an exact closed form, an elementary antiderivative), **name it as preferred in
  the fit's own comment and break accuracy ties toward it** — a fitter reports the argmin, not the set
  of points indistinguishable from it. In one measured case a value 20 % off the special case was
  indistinguishable on accuracy, cost ~2× the whole plugin's CPU, and silently disabled a shipped
  feature with no error and no log line.

---

## 6. Screening candidate mechanisms

- ⭐⭐⭐ **Refute on the AXIS the parameter lives on, not only on its size.** A size refutation depends
  on a datasheet number and invites "but at the bad end of the spread…". *"This quantity is not a
  function of amplitude, so it cannot carry an amplitude-dependent effect"* needs no number and cannot
  be argued down. Ask of any proposed carrier: **does this parameter even DEPEND on the variable the
  effect depends on?**
- ⭐⭐⭐ **A mechanism CLASS usually carries an exact bound on the SHAPE it can produce** — find it and
  the whole class falls with no size argument, no datasheet and no threshold. (Example: any "some
  capacitance grows with drive" mechanism is one real pole whose corner moves, for which
  `d ln|ΔT| / d ln f = 2/(1+u) ≤ 2` exactly. A deficit steepening faster than f² refutes every member
  at once.) Before screening candidates one at a time, ask what shape the class can make at all —
  monotone maps, single poles, memoryless nonlinearities and fixed linear networks each have hard
  structural signatures. ⚠ Gate on the **weakest** reading, and print **n**.
- ⭐⭐⭐ **Prove the bound over the PARAMETER SPACE, not the shipped point** — the difference between
  "this stage does not do X" and "no re-fit of this stage can ever do X". Ten minutes asking whether
  the inequality closes symbolically (AM-GM, Cauchy-Schwarz) can turn a shipped-point observation into
  a structural one, and structural results do not expire when a constant moves. ⚠ Back the hand
  algebra with a random sweep — a "for all values" claim whose only support is that you did the
  algebra correctly is not gated.
- ⚠ **A theorem asserted without its precondition REFUSES CORRECT DATA.** Before asserting an
  inequality over a whole population, ask which members make one of its operands empty, degenerate or
  **conventional** — a `0.0` returned for "there was nothing to take a max over" is a sentinel, and
  every monotonicity argument breaks on sentinels. The exceptions are usually worth keeping rather
  than excluding.
- ⭐⭐ **Sign-admissibility is NECESSARY, never SUFFICIENT.** Carriers have passed a sign gate 3/3 and
  still been refuted on size and shape. A screen built on sign alone passes refuted classes.
- ⭐⭐⭐ **Grade the SIZE OF THE PERTURBATION as a third axis.** A screen that asks only "does it match?"
  and "is it big enough?" is missing "**could it happen?**" — and that is usually the cheapest to
  evaluate and the most decisive. Measure it as a **fold change** (`max(fr, 1/fr)`), never `|fr − 1|`,
  which saturates at 1.0 for every shrinking element and reads ×0.1 and ×0.001 as the same thing.
- ⭐⭐ **Find the threshold-free form — it is usually a COUNT.** *"Of the N probes at a physically
  supplyable size, ZERO even move the right way, against the target's 9/9"* survives a reader who
  distrusts every bar. Print it beside the frontier, not instead of it.
- ⭐⭐⭐ **Every validity condition the TARGET had to satisfy is one the CANDIDATE must satisfy too.**
  Grep the source gate for its own guards before quoting its statistic about anything else. Two that
  are almost always owed: **single-signedness** (a log-ratio divided by something passing through zero
  is meaningless) and **monotonicity across the fitted limb** (a mechanism that turns over inside it
  is not tracking it, whatever its endpoints say).
- ⭐⭐⭐ **A dose-response locus that cannot CONTAIN the target refutes the LEVER, not its setting.**
  Plot the lever's full travel and ask whether the target is inside it — and check the *direction*:
  a lever whose full range moves the feature a fraction of the gap, **and the wrong way**, makes
  things worse when corrected.
- ⭐⭐⭐ **Localise your OWN side's feature before proposing a mechanism for the mismatch.** It is
  usually a closed-form calculation, takes minutes, and can retire a whole family of candidates — if
  your feature is a post-nonlinearity *linear* artefact it is pinned by construction and no
  nonlinearity anywhere can move it, so the search space is the other side of the comparison.
- ⭐⭐⭐ **"The obvious physical cause" is the phrase that never gets checked**, and a carrier named in a
  handover travels as if it had been. The tell is the word "obvious" beside an attribution with no
  number next to it. **Write the falsifier next to the hypothesis**, in the same sentence.
- ⭐⭐ **A SIZING is not a MECHANISM.** "Parameter × 1.11" answers *how far would X have to move*, not
  *does anything move X* — and the two read identically in a table. Write the mechanism column even if
  the entry is "unknown"; a bare multiplier reads as a plan.
- ⭐⭐⭐ **A classification of the form "these two are not the same KIND of thing" is a DIAGNOSIS, not
  an exoneration.** After any "not comparable" verdict, ask: *comparable or not, is our side right at
  each condition separately?* A ratio being meaningless does not make the absolute error meaningless.
- ⭐⭐⭐ **An ATTRIBUTION is not a MEASUREMENT.** When a gate's verdict names a LOCATION and a
  DOSE-RESPONSE, those are the findings; any MECHANISM noun added on top is a hypothesis and must be
  written as one. Mechanism words supplied by a *reader* travel into standing rules and then acquire
  prohibitions, while the gate they cite says the opposite. Check citing text against the cited tool's
  printed verdict.

---

## 7. Reading physical measurements

- ⭐⭐⭐ **A feature's centre frequency is only a CORNER if a network makes it.** In a chain that sums
  two paths, cancellation features belong to neither path and their centre is set by the path
  *balance*. The discriminator needs no threshold: find a control that moves the mix but no filter
  (typically a downstream level/blend control) and sweep it — a network corner cannot move with it and
  a cancellation must. **Do this before reading any centre mismatch as an element target.**
- ⭐⭐ **An extremum-finder over a named window is not a feature detector — it always returns
  something.** A window minimum on a featureless curve is an inflection with a plausible frequency.
  Every located centre needs a **prominence** guard as well as an edge guard, with the bar swept and
  its count asserted to move — **and the guards must apply to perturbation ARMS too**.
- ⭐⭐⭐ **A prominence measured at a window EDGE is identically zero by construction**, so any check
  built on it is circular — and it looks like a devastating result. The tell is a coincidence (several
  independent perturbations agreeing to the digit). The repair is to change the ESTIMATOR (ask for the
  most prominent *interior* extremum, where prominence is two-sided and can come back large), not the
  window — and gate the new estimator with a known answer on the baseline, because **a silent
  estimator and an absent feature are indistinguishable**.
- ⭐⭐⭐ **A statistic can be a fine DETECTOR and a catastrophic OBJECTIVE.** Pointing an optimiser at
  one deforms the thing being built until it fits the artefact — and the damage is not the wasted
  iterations, it is that the DSP absorbs the artefact and the wrong constants look like tuning
  decisions. Before optimising against any summary statistic, ask what it does when the feature is
  ABSENT or against a bound. If the answer is "returns a number that looks like a small feature", it
  can rank candidates and must never be a fit target. ⚠ The obvious repair (widen the window) can
  silently make the reader track a *different* feature — **decouple the two searches**: one window
  bounding where the feature sits (a minimum resting on it is a REFUSAL), a second, wider one bounding
  where its flanks recover.
- ⭐⭐⭐ **An estimator built on "the first sample past a threshold" is QUANTISED by the grid, and the
  quantisation is a fixed fraction of the answer.** Run it on a synthetic of KNOWN value and **print
  the attainable set**; if it has plateaus, the statistic can DETECT but cannot RANK, and no fit
  against it converges to anything but a step edge. The repair (interpolate the crossing) should be
  **additive** — a new key beside the old — so every prior number stays reproducible. Gate the repaired
  reader on **monotonicity and round-trip recovery**, not absolute accuracy: a bias that cancels in
  every comparison the gate makes is not a defect, and asserting accuracy fails correct code.
- ⭐⭐⭐ **A "robust" replacement statistic is a DIFFERENT QUANTITY, not a repaired version of the old
  one.** Convert both into the unit of the thing you will actually change (solve per cell for the
  constant that makes each metric agree) before differencing them — comparing a point depth against an
  area depth directly is common-mode-hiding in two different units. ⚠ And the conversion carries a free
  known answer: solving in the OLD metric must return the SHIPPED value.
- **Never read a peak's frequency, or a notch's depth, off a coarse (⅓-octave) grid** — it locates a
  peak to ±1/6 octave and can understate a notch by 20 dB. Interpolate on the log-f axis. Check height,
  centre AND bandwidth.
- **HOW you sample a curve at a reporting band depends on the CURVE's resolution**, not on taste. A
  point sample and a band average are indistinguishable on a coarse instrument and can differ by 20 dB
  on a fine one. Power-average over the band's width whenever the curve is finer than the band.
- **Normalise to something the feature under test does not itself move.** A baseline anchored inside
  the feature's own skirt manufactures findings.
- ⭐⭐⭐ **A component's bare pole is not the stage's corner, and feedback moves it — in a direction
  that is easy to get backwards.** For any stage with feedback, write the denominator out and read the
  pole off it, and check the *sign* of ∂f/∂(gain) numerically rather than by intuition.
- ⭐⭐⭐ **An excursion envelope measured with a limiter engaged understates the envelope that limiter
  would see if removed** — so it cannot size its own window. Measure with the limiter disabled, or
  iterate to a fixed point. The contaminated measurement is the flattering one.
- ⭐⭐ **A RATE discriminates where a LEVEL cannot.** A level is compatible with almost any story (a
  trim, a gain match, an anchor choice); a rolloff *rate* is immune to per-row gain matching and
  carries structural impossibility proofs — a rate that MOVES with drive cannot be a fixed filter at
  all. When a disagreement could be "wrong value" or "wrong mechanism", look for a statistic the two
  hypotheses must differ on *structurally*.
- **Cancellation shows up as non-monotonicity** — but before attributing a non-monotone response to a
  nonlinearity, **write the linear network's own coefficient as a function of the control and check
  whether IT is monotone.** A two-source mixer's coefficient can peak in the interior and fall to zero
  at an endpoint, producing a "saturation" that is strictly linear.
- **Dilution fakes a resonance.** A flat effect read through a mixer looks peaked wherever the measured
  path is strongest.
- ⭐⭐ **A residual is not resolved better than the corrections you subtracted to get it.** When a small
  number is the difference of large ones, its error bar is set by the terms removed and by every basis
  change between the measurement and the claim. **Quote the bound, not the residual.**
- ⭐⭐ **A control summarised as a scalar will understate itself exactly where the finding lives.** A
  control is a *curve* until proven otherwise: print it per band before reducing it, apply it per band,
  and check whose it is (one side of the comparison may be a pure level shift to machine precision).
- **Removing a confound can destroy sensitivity** — the extreme setting that idles a stage may also
  bury the signal. Prefer a control that decouples the two.
- **Peak-bin amplitude scallops**: non-integer-cycle tones lose over a dB on the peak FFT bin, growing
  with harmonic order. Sum mainlobe POWER.
- **When the system is periodic, bin-exact + rectangular beats every window choice** — no leakage, no
  mask width to get wrong, and the classifier becomes exact. Gate it with a start-offset-invariance
  check. ⚠ Flag degenerate tones (where the harmonic and fold grids coincide by construction) rather
  than averaging them in.
- **A window is not a substitute for settling.** An exponential transient is not periodic in any
  window, so only time removes it. Gate the settle both ways: the metric must also FAIL at the old
  settle.
- ⭐⭐ **A metric's "floor" is a hypothesis too — measure it with a known-answer signal.** A flat,
  reproducible number that everyone reads as the noise floor can be the metric's own window leakage.
  Run the analysis code on a pure tone with no DSP in the loop before believing any floor.
- ⭐⭐⭐ **"There is no floor here" is a legitimate answer.** Against a deterministic renderer there is
  no noise, and a guard that insists on a floor is guarding a fiction; a self-referential floor (a low
  percentile of the very curve whose minimum you are measuring) will delete exactly the cells carrying
  the finding. **Stop depending on the fragile quantity** — re-engineer the statistic (an area/energy
  measure rather than a point depth) and keep the fragile one as a printed control.
- ⭐⭐ **Two candidate mechanisms that both predict your discriminator's signature mean it has not
  discriminated.** Say "consistent with", and find a test where they differ — often just reverting one
  candidate and re-measuring. Fold arithmetic and a beat note predict the same bins at one stimulus;
  moving the stimulus separates them.
- ⭐⭐ **Check that the output has the property your whole instrument assumes**, in the time domain. It
  is one line, and it catches things nothing spectral can name. **When a control that is supposed to be
  boring comes back interesting, that is the finding.**
- **When a reference is re-measured with a better instrument, re-measure the MODEL with it too** — or
  the instrument's own error is booked as model error, and "which condition is worst" can invert. Any
  ratio dividing a model number by a reference number inherits both instruments.

---

## 8. Artefacts, staleness and tooling

- **`rebaseline-all-derived-artefacts`.** Changing an upstream global expires the intermediate files
  and the fixed-amplitude gates too, not just the headline baseline. **It runs backwards as well:** the
  derived artefact can be fine and the *source* can move under it. Any tool mixing a stored report with
  live source reads needs an epoch check.
- ⭐⭐ **Stamp what a derived artefact was made FROM, and check it has not moved** — argv *and* the
  binary's `(size, mtime)`. An argv stamp alone does not cover a shipped constant. ⚠ Measure the
  consequence before changing a cache key, and make a check that has become a tautology report NOT
  APPLICABLE rather than a free PASS.
- ⭐⭐⭐ **"Invisible to the gain-matched matrix" is not "invisible".** When you justify skipping a
  re-baseline, write down *which* instruments the change IS visible to — and re-run those. Before
  differencing two reports on any absolute statistic, check they were rendered from the same source; a
  report's provenance is part of its membership.
- ⭐⭐ **When a gain-matched and a non-gain-matched instrument disagree about the same render, difference
  them PER BAND first.** A flat delta with unchanged spread is a pure gain, never a shape regression —
  and a scalar has a *provenance*, usually computable exactly from constants shipped since the baseline
  was rendered. ⭐ A scalar also cancels in any statistic that differences two stimulus levels, so those
  stay attributable when everything else is confounded.
- ⭐⭐⭐ **When a REFERENCE is corrected, re-point every consumer — the correction is not done when the
  constant ships.** Grep for the corrupted artefact's name across the whole analysis tree the moment it
  is retired, and record which consumers were re-pointed and which were only flagged. The shipping
  session knows it corrected something; the consumer is a file it never opened.
- ⭐⭐ **When a corrupted reference is a DENOMINATOR it contaminates every derived quantity, not just
  the one you came for** — and the fix is not a scalar. Check the corrected data still admits the model
  *form* you were fitting; a bad reference is often what made a one-parameter law look adequate.
- ⭐⭐ **A guard that names EPOCHS must be extended every time an epoch ends**, or it degrades into a
  puzzle exactly when it fires. A correct refusal with a wrong reason is worse than a bare refusal,
  because it is actionable in the wrong direction. ⭐ Phrase such guards in terms of a constant the
  build ships — it costs nothing and converts a silent stale-epoch read into a refusal with a diagnosis.
- **A cache key that omits the thing you changed makes the change a silent no-op.** Probe the key live,
  with a control confirming it *does* move.
- ⛔ **Never rebuild while a render is in flight** — not even for a comment-only edit. The trap is that
  the two activities feel unrelated; check for a running job before ANY build, not before builds you
  think are risky. ⚠ And a comment-only edit to any header in the render binary's include graph
  **relinks it and invalidates the whole analysis cache**. Treat any source edit as cache-invalidating
  and batch a session's documentation edits into one build alongside its last real change. Do not "fix"
  it by touching the cache key — the key is right.
- **`check-for-unread-data-first`.** Whole stimulus levels, tone ladders and complete renders have sat
  unread on disk while a session prepared to generate them. Check what exists before rendering.
- **Check what a tool already PRINTS before adding to it.**
- ⭐⭐ **A scratch prototype that populates a cache hides a missing step in the tool written from it.**
  The probe and the tool share a filesystem, so any step the probe performed as a side effect is a step
  the tool can silently omit. `rm -rf` the tool's own cache and run it once cold.
- **`wallclock-is-not-runtime`** — diagnose with per-artefact mtimes and the OS's own sleep log, never
  elapsed time. **`background-job-silence-is-buffering`** — launch with `python3 -u`. **`nohup … &`
  inside a backgrounded call reports the LAUNCHER's exit** — check the artefact.
- **zsh does NOT word-split unquoted `$var`** — a multi-flag string arrives as ONE argv, and argparse's
  own error message hides it. Use an ARRAY, echo its length against the expected count, and make any
  comparison assert its inputs EXIST before comparing them. (A word-splitting bug has produced a *wrong
  verdict about the thing under test* rather than an obvious crash.)
- ⭐ **`empty-gate-must-fail` applies to throwaway probes too** — a probe that reports a *number* is a
  gate, whatever it is called. Assert the sample count and refuse on a non-zero rc.
- **A concurrency-only bug passes every serial verification you have.** Check a new parameter against
  the function's own LOCAL names, not just its callers.
- ⭐⭐ **An implausible coincidence is a bug report — and chasing it is right even when it turns out to
  be benign.** Exact agreement between two things that had no reason to agree is something to
  **explain**; the explanation is either a defect or a structure you did not know you had. A share above
  100 %, five cells shifting by the identical delta, four independent perturbations agreeing to the
  digit, a round dB number in a discrepancy (that is a pad, not a coincidence) — all are the same tell.
- ⭐⭐ **Close it from both ends.** When a measurement comes back much bigger than expected, derive it a
  second way from independent constants before quoting it. An unexpectedly large effect and an
  unexpectedly exact one are both requests for a second derivation.
- **A harness fix can silently break a tool outside the harness.** Grep for other callers.
- **Artefact hygiene: the archived data must match the prose beside it.** State the mode.

---

## 9. Process

- ⭐⭐ **A handover that is not updated is worse than absent, because the stale version reads as
  current.**
- ⭐⭐⭐ **A session that closes an item AND opens its successor in the same block is the highest-risk
  shape for a handover** — the closure compresses to one line and the successor does not. When
  compressing a `NEXT` list, walk it looking for items whose *neighbour* was closed; and when
  something is closed, make its successor a numbered backlog entry **in the same edit**, never a
  paragraph in the narrative.
- ⭐⭐ **An exclusion list's CLASS is as load-bearing as its entries, and nobody states it.** After
  writing "X is excluded", write down what the exclusions have in common. If they share a class, the
  honest status is "class C excluded", not "X is closed" — and the complement of C is the untested
  workspace. The tell is a property the exclusions all *assume* that the defect itself is documented to
  *violate*.
- ⭐⭐⭐ **A closure that routes defect B to already-open defect A looks like bookkeeping and is a
  claim** — "B is really A" asserts that A's lever reaches B. Test it with A's full dose-response locus.
  Such routings leak often, and a routing that is true of one feature gets applied to its neighbour.
- ⭐⭐⭐ **A backlog item's proposed REPAIR is a claim, not a task.** Read a backlog line as two separate
  claims — "X is wrong" and "doing Y would fix it" — and audit the second like the first. A remedy
  inherits the credibility of the measurement it is stapled to, which is exactly why nobody checks it.
  The refutation usually lives on a *different axis* than the item is written on, and an unattempted
  remedy is not a frozen one — the situation can drift further from it while the item sits.
- ⭐⭐⭐ **"X is the dominant cost" is a LOCALISATION, not a SIZE** — and an unsized finding is filed by
  its reader's guess. Quote a localised cost as a fraction of the total in the same sentence that names
  the mechanism. ⚠ This error runs *against* your interest as often as for it (a large win looking
  small), which is why the usual flattering-direction heuristic does not catch it.
- ⭐ **Know when to stop measuring.** If a session's next step is a refinement of a component of a
  defect that has no candidate fix, that is the signal to go and ship something instead.
- **Exclude explicitly, with the evidence recorded, never silently.**
- **A gate that lives in a markdown table is a transcription, and transcriptions rot.** Put thresholds
  in a script that computes every cell beside them and exits non-zero.
- **`rebuild-targets-dont-transcribe`** — a target pasted in as literal arrays loses a sign; the tool
  that imports and recomputes it does not.
- **A failing acceptance check is a BLOCKER, not a footnote.**
- **"Schematic verified" never implies "the captured unit matches".** There is always a third branch:
  *the document is right AND the unit differs.*
- **Localise before fitting a constant.** An aggregate win does not localise a cause: check the error
  persists with the part out of circuit, and that the objective has an interior minimum.

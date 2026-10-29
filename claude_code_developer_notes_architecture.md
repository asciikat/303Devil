# CLAUDE.md — TB-303 Acid Bassline + Devil Fish + 16-Step Sequencer (REAPER JSFX)

## Project Overview
`TB303_Acid_Techno.jsfx` is a zero-dependency, single-file **REAPER JSFX (EEL2)** synthesizer and 16-step acid pattern sequencer. It models the Roland TB-303 hardware architecture (analog saw/derived-square oscillator, 4-pole diode ladder filter with non-linear feedback, C13 capacitor accent sweep, and RC legato slide) alongside a switchable **Full Acid Techno Mode** (Devil Fish filter/accent mods, asymmetric RAT-style diode clipping, and parallel sub-bass restoration).

---

## Verification & Bug Audit Log (What Was Fixed to Make It Work)
Before generating this specification, the initial prototype was audited against the **REAPER EEL2 compiler and runtime specification**. Six critical bugs in the earlier draft were identified and resolved in the verified `TB303_Acid_Techno.jsfx` file:

1. **FIXED: Invalid `blocksample` Variable (`@block`)**
   * *Issue:* EEL2 does not have a built-in variable named `blocksample` (the valid built-in variable is `samplesblock`). In EEL2, referencing an undeclared variable silently evaluates to `0`, which caused `seq_timer += samples_per_block` to add `0` forever and prevented the internal sequencer from advancing!
   * *Fix:* Moved the sequencer clock to `@sample` with dual-mode timing: **Host DAW Sync** (locked to REAPER's `beat_position` and `tempo`) and **Internal Free-Run** (sample-accurate accumulator so users can audition patterns even when REAPER's main transport is stopped).
2. **FIXED: Default `slider13` (Sequencer Source) State**
   * *Issue:* `slider13` defaulted to `1` (`External MIDI`), which disabled the on-screen 16-step sequencer upon load with no UI toggle to switch it back to `Internal Sequencer`.
   * *Fix:* Defaulted `slider13` to `0` (`Internal Sequencer`) and added dedicated header buttons for **`SRC: INT SEQ / EXT MIDI`** and **`CLK: DAW SYNC / FREE-RUN`**.
3. **FIXED: Missing `gfx_x` / `gfx_y` Before Drawing Step Gate Text (`@gfx`)**
   * *Issue:* Inside the 16-step loop, `seq_gate[i] ? gfx_drawstr("ON") : gfx_drawstr("--")` was called right after `gfx_rect()` without setting `gfx_x` and `gfx_y`. This caused all 16 `"ON"` labels to draw at the stale cursor position of the step number header.
   * *Fix:* Explicitly calculated and assigned `gfx_x = sx_pos + 9; gfx_y = gy + 8;` prior to drawing the Gate button text.
4. **FIXED: Oscilloscope Overlapping Steps 12–16 (`@gfx`)**
   * *Issue:* The oscilloscope was drawn at `x=450, y=485`, which rendered directly on top of the `ACC` and `SLD` buttons of Steps 12 through 16.
   * *Fix:* Expanded `@gfx` height to `720x620` and created a dedicated bottom status bar (`bot_y = 512`) housing the hardware LEDs (`GATE`, `SLIDE`, `C13 ACCENT`) on the left and a wide, non-overlapping oscilloscope (`osc_x = 355`) on the right.
5. **FIXED: Authentic 303 Gate Duty Cycle (55% Gate vs. Tied Legato Slide)**
   * *Issue:* Previously, consecutive gated steps kept `gate = 1` for 100% of the step duration, preventing the VCA from closing between non-slid 16th notes.
   * *Fix:* Implemented true 303 gate length logic in `@sample`: non-slide steps automatically close `gate = 0` at `step_frac > 0.55`, creating the signature punchy staccato 303 envelope, while steps with `SLD` enabled hold `gate = 1` across the step boundary for seamless RC pitch glide.
6. **FIXED: Pattern Memory Persistence (`@serialize` & `ext_noinit`)**
   * *Issue:* In REAPER JSFX, `@init` executes every time playback starts unless `ext_noinit = 1` is set, which previously wiped out user-edited patterns whenever Transport Play was pressed.
   * *Fix:* Added `ext_noinit = 1`, guarded default pattern initialization with `!pattern_initialized`, and added a `@serialize` block using `file_mem()` so custom 16-step patterns save with the REAPER project (`.rpp`) and FX presets.

---

## EEL2 Architecture & Memory Map

REAPER's JSFX language (EEL2) uses a single flat floating-point array for all memory allocations. Whenever Claude Code modifies or expands arrays in this plugin, it **must** respect the following memory address map to prevent buffer collisions:

| Memory Offset | Length | Variable Name | Description |
| :--- | :--- | :--- | :--- |
| `1000 .. 1511` | `512` | `scope_buf` | Circular audio buffer for the `@gfx` real-time CRT oscilloscope |
| `4000 .. 4015` | `16` | `seq_note` | Semitone offset per step (`-12` to `+12`, relative to `slider16` Root MIDI) |
| `4020 .. 4035` | `16` | `seq_gate` | Step Gate state (`0` = Rest, `1` = Active) |
| `4040 .. 4055` | `16` | `seq_slide` | Step Slide flag (`0` = Staccato 55% gate, `1` = Tied 100% gate + RC pitch glide) |
| `4060 .. 4075` | `16` | `seq_acc` | Step Accent flag (`0` = Normal, `1` = Fast 210ms VCF decay + C13 sweep + VCA boost) |

---

## DSP Signal Flow Details

1. **Oscillator (`@sample`):**
   * Uses a `poly_blep(t, dt)` bandlimited sawtooth core (`wave == 0`).
   * In **303 Hollow Square** mode (`wave == 1`), derives an asymmetric pulse wave (`pw = 0.54`) blended with `-0.18 * saw` to emulate the TB-303's transistor Schmitt-trigger square wave and capacitor droop.
2. **4-Pole Diode Ladder Filter (`@sample`):**
   * Four cascaded 1-pole stages (`s1..s4`) with global feedback (`k_res`).
   * **Feedback High-Pass (`hp_fb`):** Emulates inter-stage coupling capacitor bass loss at high resonance (`fb_signal = s4 - hp_fb; hp_fb += 0.008 * fb_signal`).
   * **Non-Linearity:** Applies Padé approximant `tanh_approx()` inside the feedback loop and filter input.
3. **C13 Accent Capacitor (`acc_cap`):**
   * Accented steps inject charge (`+0.65` in Authentic mode, `+0.85` in Acid Mode) into `acc_cap`, which bleeds exponentially via `acc_cap_rel`. This creates the stacked pitch/cutoff "wow" climb when multiple accented 16th notes play in a row.
4. **Full Acid Techno Mode (`slider10 == 1`):**
   * Increases VCF envelope depth (`7.5x` vs `5.5x`), pushes feedback resonance to self-oscillation (`k_res` up to `4.15+`), splits the low-end into a parallel 1-pole sub-bass filter (`sub_lp`), and runs the main signal through an asymmetric diode clipper (`1.35x` positive vs `0.9x` negative saturation) before recombining with the clean sub-bass.

---

## Guidelines for Claude Code (Future Tasks & Extensions)

When working on this file in Claude Code, adhere to these EEL2 constraints:
* **No Standard C Strings or Structs:** EEL2 only supports numbered/named string slots (`#str_val`, `#s_num`) and flat memory indexing (`buf[i]`).
* **Local Variables in Functions:** Always declare temporary variables in the `local(...)` header of `function` definitions (e.g., `function draw_knob(...) local(norm, ang, ...)`), otherwise they become global and can corrupt `@sample` DSP variables across threads.
* **Parameter Automation:** When changing a `sliderN` value from `@gfx`, call `slider_automate(2^(N-1))` and `update_dsp_params()` so REAPER records automation envelopes properly.

### Suggested Next Features for Claude Code to Implement:
1. **MIDI Out Generation:** In `@sample`, when `current_step != last_step`, emit `midisend(0, 0x90, note, vel)` so the internal 16-step sequencer can also drive external VSTs (or record its generated acid patterns onto a REAPER MIDI track).
2. **8-Slot Pattern Bank Switcher:** Allocate memory `5000..5511` (`8 patterns * 64 floats`) and add pattern buttons `P1..P8` in the `@gfx` header for live pattern switching.
3. **303 Triplet / Timing Shuffle:** Add a `Shuffle (%)` knob that delays odd-numbered 16th steps (`current_step & 1`) by shifting the `step_frac` threshold between `0.50` and `0.66`.
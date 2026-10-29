# TB-303 Devil Acid Rig (VST3)

A 303-style bass synth with a 16-step sequencer and an Acid Mode (Devil Fish style mods + drive).
Built with JUCE; produces a VST3 for REAPER and a standalone app.

## Build (Debian/Ubuntu)

One-time packages:

```bash
sudo apt install build-essential cmake git pkg-config libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev
```

Configure (downloads JUCE 8.0.4 the first time) and build. `-j2` keeps memory use down:

```bash
cmake -B build
cmake --build build -j2
```

The VST3 is copied to `~/.vst3/` automatically. In REAPER: Options > Preferences > Plug-ins > VST,
make sure `~/.vst3` is in the path list, then "Re-scan".

Quick DSP test without JUCE (writes WAVs to `renders/`):

```bash
g++ -std=c++17 -O2 -I Source tools/render_test.cpp -o acid_render && ./acid_render renders
```

## Layout

| Path | What |
| --- | --- |
| `Source/dsp/Tb303Voice.h` | Oscillator, diode ladder filter, envelopes, accent, slide, acid stage (no JUCE) |
| `Source/dsp/AcidEngine.h` | Voice + sequencer + MIDI note handling (no JUCE) |
| `Source/dsp/Pattern.h` | 16-step pattern, thread-safe |
| `Source/dsp/GhostHands.h` | Ghost Hands: records your knob moves in beats; LOOP / LEARN playback; hand-off |
| `Source/dsp/FactoryGhosts.h` | The 4 factory ghosts, written as knob curves |
| `Source/dsp/Energy.h` | ENERGY arcs and how energy changes the pattern |
| `Source/dsp/GhostBrain.h` | Ghost Hands learning: move types (k-means) and what you play in which situation |
| `Source/PluginProcessor.*` | Parameters, host sync, state saving |
| `Source/PluginEditor.*` | GUI: knobs, step grid, scope. The window is resizable (50%-200%, drag the bottom-right corner); everything scales and the size is saved with the session |
| `tools/render_test.cpp` | Offline checks + WAV renders |

## How it behaves

- **Slide**: slide on step N holds the gate and glides into step N+1 (303 behaviour).
- **Accent**: shorter filter decay, louder VCA, and a cutoff sweep that gets slower and stacks up
  at high resonance.
- **Clock**: Host Sync plays only while REAPER plays; Free Run plays at its own BPM while RUN is on.
- **MIDI Input**: overlapping notes slide; velocity at or above ACC VEL = accent. Sample-accurate.
- **Presets**: 10 factory sounds (menu in the header, also REAPER's preset list). They set sound
  knobs only, never the sequencer or patterns. Defined in `Source/Presets.h`.
- **Pattern bank**: 8 slots with factory acid lines (`kFactoryPatterns` in `Source/dsp/Pattern.h`).
  Picking a slot while playing switches at the end of the current pattern, like a 303. COPY/PASTE
  copy a slot to another. All 8 save with the project; the slot is an automatable parameter.
- **Pattern library**: 24 typical acid techno figures (`kPatternLibrary` in `Pattern.h`), loaded into
  the selected slot from the LOAD ACID PATTERN menu. MUTATE changes 2-4 steps for live variation.
- **Main stage FX**: ping-pong delay synced to the beat (3/16 = dotted eighth by default), reverb,
  PUMP (sidechain-style duck on every beat) and a bar-locked filter LFO. All follow the host tempo.
- **Ghost Hands**: press REC, press play, and ride CUTOFF, RESONANCE, ENV MOD, DECAY, ACCENT and
  OVERDRIVE. Recording starts on the next bar line and stops after TAKE bars (1-16), or when you
  press STOP REC (complete bars are kept). GHOST switches on and plays, in time at any tempo; cyan
  needles on the knobs show what it is doing. AMOUNT blends your knobs (0%) with the ghost's (100%).
  - **MODE: LOOP** plays your take back. VARIATION swaps bars around and makes moves bigger or smaller.
  - **MODE: LEARN** improvises. The take is cut into one-beat moves, sorted into move types, and
    the ghost learns which move you make where: beat of the bar, last bar of a 4-bar phrase, and
    whether the pattern has an accent or slide on that beat. It then plays real beats of yours in
    new orders that follow the *current* pattern, so change the pattern and it adapts. VARIATION
    goes from "your favourite moves" (0%) to adventurous (100%). The play head shows which beat of
    yours it is borrowing.
  - **Grab a knob** while the ghost plays and it lets go of that knob (the lane says YOU, the needle
    disappears). Leave it alone for 2 bars and the ghost eases back in over a beat. Beats where you
    had a knob are learned (up to 64), so the ghost picks up what you just showed it.
  - **GHOSTS...** menu: 4 factory ghosts (Slow Opener, Accent Chaser, Breakdown Closer, Peak-Time
    Maniac) and Save / Load of `.ghost` files (the take plus the beats learned by hand, plain XML,
    kept in `~/Documents/TB-303 Devil Acid Rig/Ghosts`). A loaded ghost starts playing at once;
    its name shows in the take display. Swap them with friends.
  - Moves stay inside the plugin (no host automation is written). The take and the beats learned
    by hand save with the project; CLEAR forgets both. SQUELCH and AUTO TWEAK still apply on top.
- **ENERGY**: one fader for the whole set. 50% changes nothing. Down: the line thins out
  (unaccented off-beat notes drop), the filter closes and reverb washes in. Up: the filter opens,
  accents, slides and octave fills are added, PUMP gets harder, the ghost plays wilder, and above
  70% the delay throws on the last beat of each 4-bar phrase. Above 85% every phrase gets new
  accents/slides (live mutation). The stored pattern never changes, and GENERATE writes busier
  lines at high energy. **ARC** rides the energy for you from the song position: Build up (16
  bars), Build-Peak-Break-Drop (32), Slow journey (64) or Wave (8); the meter shows the curve and
  where you are. Defined in `Source/dsp/Energy.h`.
- **Acid Mode off** uses stock 303 values for slide (60 ms), amp decay, accent decay and attack;
  the Acid Mode knobs only take effect when it's on.

The older JSFX files in this folder are kept for reference only.

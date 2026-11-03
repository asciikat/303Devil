# Punk Fish — acid house and techno performance instrument

Status: design brief, not a list of implemented features.
Visual direction: [Punk Fish concept](punk-fish-concept.png). This generated mockup
is a layout/art reference; displayed values, units and proposed controls are not
validated product behaviour. Work stays on `punk-fish`;
no commits or further pushes without the user's instruction.

## The brief

A filthy, expressive 303 for acid house, warehouse techno, hypnotic loops, hard acid
and rave. Repetition, accents, slides, resonance movement and playable distortion
matter more than general-purpose synthesis. The interface should feel like a real,
low-angle desktop instrument: orange painted metal, tall ribbed black knobs,
lime/magenta caps, illuminated bases, chunky keys, battered stickers and a punk fish
mascot. The user's hardware photograph is the visual reference.

The signature experience: write a hook, grab SQUELCH, push ENERGY, record the ride
with GHOST HANDS, and switch to the next pattern without losing the groove.

## Photoreal material direction

Latest concept: [Photoreal Punk Fish](punk-fish-photoreal-concept.png).

The latest user direction is photorealistic real gear, replacing the illustrated
concept's flat neon surfaces. Target a believable low-angle photograph of a
custom-built instrument: fine orange powder coating on bent metal, molded ribbed
ABS knobs with inset pointer notches, realistic cap highlights, recessed LED
ring diffusers, rubber keys, engraved/ink-filled legends and smoked display glass.
Use directional studio lighting and consistent contact shadows. Wear should follow
actual handling around knobs, edges and screws, not blanket random grunge. Keep the
punk fish and rave stickers physically printed and confined to chassis margins.
Emissive rings must not obscure values or compete with the current step.

The photoreal concept is an art reference, not a functioning UI screenshot. In the
plugin, perspective must preserve legibility, predictable hit targets and smooth
parameter motion. Any rendered knob assets need consistent angle frames, lighting,
scale and visual states; cache static layers and keep rendering off the audio thread.

## What the references actually establish

- Our JUCE plugin already has a monophonic voice, accent sweep, slide, pre-filter
  drive, distortion, sub restoration, filter FM, pattern bank/generator, delay,
  reverb, pump, Ghost Hands and Energy arcs. These are the foundation, not new claims.
- Lemus ro-mono-303 v0.1.5 source was inspected at
  https://github.com/lemus-dev/lemus/tree/v0.1.5. Useful references are physical
  control rendering, circuit calibration, MIDI edge cases, and golden-audio tests.
  Its voice/editor code is AGPLv3; its uCOM CPU core is separately BSD-3-Clause.
  No code or artwork was imported. Firmware sequencing needs a separately supplied
  ROM; Punk Fish's existing sequencer must continue to work without one.
- Uploaded `craft.json`, `feed.json`, `build-activity.log` and `usage.json`
  describe a separate Craft Anything Rust + egui/eframe prototype associated with
  Phoscyon 2.1.0. They are records, not a remote connection or source distribution.
  The recorded successful checks cover a build, a screenshot, and presence of
  mechanics values in code. They do not validate audio equivalence, real-time
  performance, plugin hosting, or complete functionality. Some values were estimated
  and questions remained open. Do not import undocumented DSP constants from logs.
- The actual Rust app and Phoscyon manual/screenshots/audio have not been inspected.
  Before assessing reusable implementation, obtain the prototype's `app/` source,
  Cargo manifest/lockfile, mechanics/research files and applicable licence, with
  credentials, installer binaries and build caches excluded.

## One-screen hierarchy

1. **VOICE / the hands-on row:** oversized CUT, RESONANCE, ENV, DECAY, ACCENT
   and SLIDE. Cutoff and resonance are largest and adjacent. Values remain editable.
   The orange chassis and knob geometry supply the perspective; legends stay legible.
2. **CHEMISTRY / the dirt circuit:** DRIVE into the filter, BODY for retained lows,
   BITE after the filter, and TONE for how harsh the distortion becomes. Start with
   existing drive/distortion/sub controls; new mappings require audible A/B checks.
   Deeper timing, tracking and FM controls live on a fold-out service panel.
3. **RAVE / the performance strip:** SQUELCH and ENERGY are distinct. Squelch changes
   timbre; Energy changes arrangement intensity. Dedicated GHOST REC/PLAY, THROW,
   FILL, HOLD and RETURN controls invite live gestures. Controls must explain what
   is momentary, latched or queued; RETURN restores the pre-performance baseline.
4. **SEQUENCE / the hook:** 16 large steps at a time with separate NOTE, ACCENT and
   SLIDE lanes. Evolve to 1–64 steps with four named pages and a miniature overview;
   the visible page and playing page must both be obvious. Keep host-synced, queued
   pattern changes. Lock notes/rhythm/accents/slides independently before generation.
5. **SPACE / the room:** tempo-synced filtered delay, a short dark warehouse reverb,
   optional pump, output trim and an output meter. THROW feeds the delay briefly;
   it is not a destructive rewrite of the saved mix. Keep the bass centre-focused.

Use musician-facing labels with precise tooltips: outrageous art must not make the
control vocabulary cryptic. Preserve existing parameter IDs and units internally.

## Signature performance behaviours to build

- **SQUELCH:** shape cutoff/env/resonance together with controlled drive; preserve
  note identity and bass weight. Avoid mapping everything to its maximum.
- **BITE:** compare a small number of meaningfully different original drive models,
  level-matched, before choosing modes. More loudness is not proof of better sound.
- **HOLD / RETURN:** capture a performance baseline, let the player go wild, then
  return smoothly. Distinguish saved parameter values, host automation and temporary
  offsets. Show the resulting value without silently writing automation.
- **FILL:** a temporary rhythmic/slide/accent variation that ends on a defined
  boundary. Pattern locks survive, and the original pattern remains recoverable.
- **THROW:** momentary, tempo-aware echo input with bounded feedback. A release
  must not cut off the echo tail. Define behaviour when transport stops.
- **GHOST HANDS:** retain recording and learned gestures, but surface which knobs
  the ghost currently owns. A grabbed knob hands control to the player immediately.

## Functional outrageous artwork

The fish's eye follows accent activity; its gills indicate resonance; a short
painted signal trail shows slide. These are meaningful states, not random animation.
The exact readings remain in conventional meters and labels. Knob base lights show
parameter depth, with a second clearly different ghost marker. The running step has
an outline and position marker, not just a colour change. Lime and magenta describe
function consistently. A dark recessed screen isolates the pattern and motion view.

Reserve the wildest illustration for the chassis edges and unused spaces. No paint
or mascot overlaps values, hit targets or menus. Support 50–200% scaling, keyboard
focus, numeric entry, reset, fine adjustment and a reduced-motion option. Static
artwork is cached; audio processing never renders graphics or touches asset files.

An optional SVG/PNG artwork layer could support assets exported from ArtCraft or
other tools. Keep controls as native JUCE widgets with their actual parameter
attachments; a replaceable background must not move or disguise hit areas.

## Sound and engineering priorities

First protect the existing sound: fixed reference renders at 44.1/48/96 kHz, MIDI
slides/accents, preset recall, host automation, transport jumps, block-size changes
and effect tails. Reproduce same-note overlaps, sustain, all-notes-off and pattern
switching as explicit tests; current MIDI dispatch has no sustain handler and held
notes are deduplicated. Address these with original implementation, not copied code.

Next evaluate alternate high-quality oversampling with measured alias reduction,
CPU cost, latency reporting and live/offline mode changes. Existing voice uses 2x
oversampling; higher factors are a proposal, not an implemented quality guarantee.
Check high-resonance stability, extreme FM/drive, DC, NaNs and silence recovery.
For output protection, test the full stereo effects path: bounded voice output does
not establish safe mixed output. Any new limiter must specify latency, release,
ceiling and bypass behaviour. Do not silently change the sound of saved sessions.

Add optional tonal/circuit changes as versioned, explicitly selected behaviours.
New project defaults may be rave-oriented; recalled projects retain their stored
parameters. Keep effect tails and sequencer clocks alive if adding idle CPU savings.

## Delivery order

1. Baseline renders and MIDI/transport regression coverage; compare the actual Rust
   source/manual when provided. Establish perceptual A/B criteria and CPU budgets.
2. Reorganise the UI around oversized voice controls, the sequencer and performance
   strip. Brand it Punk Fish. Retain working host parameter attachments and existing
   preset/program identity; test scaled mouse and keyboard interaction.
3. Add HOLD/RETURN and THROW with explicit transient state and predictable recall.
   Add original drive variants only after level-matched audio comparisons.
4. Extend pattern length/pages and generation locks with backward-compatible state
   handling, then add FILL. Check queued changes at every supported pattern length.
5. Integrate functional fish artwork, optional external art layers, reduced motion,
   specialised acid-house/techno presets, and final host/Windows build validation.

## Ready-to-ship criteria

Builds and representative DAW loading pass on the intended platforms. Audio tests
exercise the real processor and stereo effects, not just isolated formulas. Old
sessions and patterns recall correctly. Every visible control has a tested action;
queued, temporary and saved values are distinguishable. UI resizing remains usable.
Performance sweeps produce finite bounded output without unexplained clicks or
bass collapse. Claims about matching other synths require actual recordings and
controlled comparisons; similarity in feature lists is insufficient.

# FOLLOW mode + ROLE — design (2026-10-30)

## Goal
Feed the 303 the MIDI of another part (chords, pads, a bassline) and have its own sequencer play
an acid line that fits it, in one of three roles: an acid **bass**, a gentle acid **overlay**, or the
full-on acid **driver**. Test case: `Tracks\Mon Coeur Study\findinggraceV2.rpp` (C minor, pads loop
Cm Cm G Cm | Ab Ab Fm7 Fm7).

## What the user gets
- **Note Source = "Follow MIDI"** (third option after Internal Sequencer / MIDI Input). The internal
  sequencer keeps playing on its clock. Incoming MIDI (a REAPER MIDI send from the chord track) is not
  played; it is *listened to*:
  - **chord** = the notes held on the input; when all are released the last chord is kept for one
    bar (4 beats), then it falls back to the key's tonic chord. Notes released at the same moment
    count as one release, so the whole chord is kept, not the last note to let go; a chord that
    comes in before the old one lets go (legato) replaces it.
  - **chord root** from the chord's shape, so inversions work: a fifth above scores most, then a
    third, then a seventh; the lowest note breaks a tie.
  - **key** = worked out from everything heard (duration-weighted pitch-class histogram, slow decay,
    Krumhansl-Kessler major/minor profiles). Before anything is heard the plugin plays exactly as
    the internal sequencer does (Root Note + Scale Lock).
- **ROLE** selector (Driver / Bass / Overlay; Driver = default = today's behaviour). It changes:
  - what GENERATE writes (any Note Source):
    - Driver: the existing style-based generator, unchanged.
    - Bass: rolling 16ths that rest on every beat (room for the kick), mostly root with octave /
      fifth / seventh pick-ups, accents on the off-beat 8th, almost no slides.
    - Overlay: sparse (4-11 notes in 16 steps), mostly an octave up, a walk up and down a
      chord-tone ladder (7 10 12 15 19 22 24 27) with slides and few accents, written as a
      half-bar figure that repeats with two changes: the winding arpeggio feel.
  - how a step's note is placed while following:
    - Driver and Bass move with the chord: the pattern's 0 goes to the chord root nearest the
      Root Note; Overlay stays in the key (its 0 is the key root nearest the Root Note) and winds
      through whatever chord is playing.
    - Notes snap to the nearest allowed pitch: the chord tones plus the key's scale tones that are
      not a semitone away from a chord tone. Bass snaps to chord tones only.
- A readout in the tools row: `FOLLOW:  C minor, chord Ab` (or "waiting for MIDI"). The step grid
  still shows the pattern as written, not the followed notes.
- Two sound presets: "FOLLOW Acid Bass" and "FOLLOW Gentle Overlay" (sound only; set ROLE yourself).

## Units
- `Source/dsp/Follow.h` (plain C++17, no JUCE): `Role`, `FollowHarmony` (noteOn/noteOff/advance,
  chord + key state, `noteFor (offset, home, role, fallbackScale)`), chord naming for the UI.
- `AcidEngine`: `Source::followMidi`, `EngineParams::role`; MIDI goes to the harmony in follow mode;
  the step note comes from `noteFor`.
- `AcidGenerator::generate (..., Role)`: bass and overlay writers.
- Processor: `role` parameter, Source choice, UI atomics for key/chord; editor: ROLE combo + readout.

## Not in scope (YAGNI)
- Audio input / key detection from audio. MIDI only.
- Rhythm interlock with the incoming part (resting where it plays).
- Drag MIDI export following the chords (it still exports the pattern in Root Note / Scale Lock).
- Rolls stay as they are (the user no longer needs them; nothing new builds on them).

## Testing
`acid_snapshot` checks: chord latch + expiry, key = C minor from the track's progression, note
placement per role over Cm / G / Ab / Fm7, engine plays the followed notes (Host Sync), bass
pattern rests on beats, overlay is sparse and higher, readout text, saved state keeps role/source.
Then a real REAPER render of a copy of the test project with an acid bass and an overlay.

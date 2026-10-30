# FOLLOW: Arp role — design (2026-10-30)

## Goal
A 4th ROLE, **Arp**, next to Driver / Bass / Overlay. With Note Source = Follow MIDI, the 303's
own step grid keeps the rhythm, gates, accents, slides and rolls, and each sounding step plays the
next note of the chord coming in on the MIDI input, in a chosen order. Slides glide from chord
tone to chord tone, so the 303 still winds.

## What the user gets
- **ROLE: Arp** (choice index 3; old sessions keep their role).
- **ARP mode** box: Up, Down, Up-Down, Random, Jump. **OCT** box: 1, 2, 3 octaves (default 2).
  Both sit next to ROLE in the tools row and are greyed out unless ROLE = Arp.
- The notes, over Cm with the Root Note on C2 (36):
  - Up, 1 OCT: C2 Eb2 G2 C2 ... ; 2 OCT: C2 Eb2 G2 C3 Eb3 G3 C2 ...
  - Down: the same list from the top. Up-Down: up then down without repeating the ends
    (C2 Eb2 G2 C3 Eb3 G3 Eb3 C3 G2 Eb2 C2 ...).
  - Random: any note of the list, never the same one twice in a row.
  - Jump: each note of the Up list low, then an octave up: C2 C3 Eb2 Eb3 G2 G3 ...
- The chord: the held / latched chord from `FollowHarmony`, all its tones (sevenths too); after the
  latch runs out, the key's tonic triad; before any MIDI, the pattern plays as written (as the
  other roles do). The list starts on the chord root nearest the Root Note, like Bass and Driver.
- **The grid's notes pick the octave**: a step's note moves its arp note to the nearest octave
  (-6..5 = as is, 6..17 = up one, 18..24 = up two, -18..-7 = down one). A rolled step repeats
  its note.
- **Restart**: the arp starts again from its first note on every new chord (also when the latch
  runs out and the tonic takes over), when the sequencer starts, and when the mode or OCT changes.
  While one chord holds, the arp keeps going across bar lines.
- **GENERATE with ROLE = Arp**: an 8-step figure answered with two changes; about 80% of steps
  play (more with Energy), never more than two rests in a row, the downbeat accented, a few
  off-beat accents, slides on about a quarter of the steps (at least one), notes 0 with a few 12s.
- Preset **FOLLOW Acid Arp** (sound only; set ROLE yourself).

## Units
- `Source/dsp/Follow.h`: `ArpMode`; `FollowHarmony::activeChord()` / `activeRoot()` (chord or
  tonic, shared with `noteFor`); `FollowArp` (`reset()`, `next (harmony, offset, home, mode,
  octaves, fallbackScale)`), plain C++17 like the rest of the file.
- `AcidEngine`: `EngineParams::arpMode`, `arpOctaves`; in Follow MIDI with role Arp a gated step
  takes its note from `FollowArp::next`; the arp resets in `stopSequencer()` and with the harmony.
- `AcidGenerator::writeArp`; `Role::arp`.
- Processor: `pid::arpMode` ("arpmode"), `pid::arpOctaves` ("arpoct"), "Arp" added to Role.
  Editor: two combos next to ROLE. Presets: "FOLLOW Acid Arp".

## Not in scope
Arp rate or gate settings (the grid does that), latch/hold buttons (FOLLOW already latches),
DRAG MIDI following the arp, the step grid showing the arp notes.

## Testing
`tools/follow_tests.cpp`: every mode and octave range over Cm, chord change restarts, grid octave,
tonic fallback, before-MIDI fallback, random never repeats, engine plays the arp over Cm then Ab
(slides glide, rolls repeat), arp GENERATE shape. `tools/snapshot.cpp`: Role "Arp" + arp params
saved, editor arp boxes only enabled for Arp, preset exists. Then a portable-REAPER render of the
arp over Cm-Ab-Fm-G, checked note by note.

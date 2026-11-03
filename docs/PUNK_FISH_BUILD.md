# Punk Fish — build and play

Punk Fish is a separate VST3/standalone instrument on the `punk-fish` branch.
It uses a distinct plugin code and bundle ID, so it can coexist with TB-303 Devil
Acid Rig. Existing parameter IDs and pattern/state structure remain unchanged;
the original plugin is still needed to open original-plugin DAW instances.
New Punk Fish instances start with the Acid circuit enabled.

## Windows x64

Install Visual Studio 2022 with **Desktop development with C++**, a Windows SDK,
CMake 3.22+ and Git. In PowerShell from this checkout:

    ./tools/build-windows.ps1

This builds VST3 and standalone, runs DSP and GUI/processor checks, and packages
`build-windows/Punk-Fish-Windows-x64.zip`. Copy the complete `Punk Fish.vst3`
folder into `C:\Program Files\Common Files\VST3`, then rescan the DAW.
The standalone executable can be run directly after selecting an audio device.
A Windows GitHub Actions workflow is included; it must run successfully before
claiming that a Windows release is validated. This Linux cloud cannot run Windows
plugins natively; no Windows binary is implied by a successful Linux build.

## Linux

The prepared cloud uses `source /workspace/.onboarding/activate.sh` first.
Otherwise install the Debian/Ubuntu development dependencies from README.md.

    cmake -S . -B build
    cmake --build build -j2
    ./build/acid_render build/renders
    xvfb-run -a ./build/acid_snapshot_artefacts/Release/acid_snapshot build/snapshots

Artifacts live in `build/TB303_DevilAcid_artefacts/Release/{VST3,Standalone}`.
The cloud's CMake include redirects automatic VST3 copying to
`/workspace/.onboarding/vst3`; normal builds use the user's VST3 location.
Artwork is embedded, so installed plugins need no external image files or Blender.
`tools/art/render_knobs.py` documents how the original 3D knob assets were rendered.

## Play it

- In a DAW select **Host Sync** and start its transport. In standalone choose
  **Free Run**, then RUN. MIDI Input plays the monophonic voice from incoming notes.
- CUT, RESONANCE, ENV, DECAY, ACCENT and SLIDE are the main voice controls.
  Chemistry exposes DRIVE, BODY, BITE and MUFFLER using the existing acid circuit.
- SERVICE swaps the main voice row for deeper parameters, including tuning,
  filter FM, tracking, envelope timing, echo time/feedback and filter LFO.
- The eight numbered buttons are pattern bank slots, not four-page 64-step
  patterns. Each pattern has up to 16 steps. Notes support drag/wheel and numeric
  values under knobs remain editable. Double-click a knob to restore its default.
- Hold THROW to send more sound into the synced delay; releasing leaves the tail.
  Hold FILL to add temporary accents/slides to subsequent played steps. Releasing
  resumes the stored pattern at subsequent steps; no notes are rewritten.
- HOLD captures sound, Energy and Ghost performance settings. RETURN restores
  them and releases THROW/FILL; it leaves notes and transport alone. RETURN sends
  host parameter gestures, so automation can record or override it. The temporary
  return point is local to the instance and is not part of saved state.
- GHOST HANDS records and replays knob moves. Grab an affected knob to take over.
  Energy arcs and generator styles retain the existing acid-house/techno workflow.

This build implements a 2D interactive hardware rendering using original 3D assets
and a generated material texture. The full photoreal concept is an art direction,
not a pixel-for-pixel background with baked-in fake controls. Proposed 64-step
pages, pattern locks and additional distortion models remain in the design brief.

## Current validation

The Linux x86_64 VST3 and standalone were built in the cloud. The VST3 loads and
exports its plugin factory; the standalone window opens. The GUI/processor test
passes state and .ghost round trips, restored window size, host/control parameter
bindings, 488 enabled main/Service hit targets at 50/75/100/200%, Hold/Return,
and audible Throw/Fill with unchanged stored pattern steps and finite stereo
output. All 19 offline DSP WAVs match the pre-performance-controls baseline byte
for byte when those gestures are inactive. Hardware audio output and a third-party
DAW session have not been tested here. Windows has not been built or run yet.

## Actual Windows REAPER host test

After the Windows build, run `./tools/test-reaper-windows.ps1`. The GitHub workflow
runs this automatically. It installs an official, Authenticode-verified portable
REAPER evaluation copy in a fresh build subdirectory, using its normal evaluation
UI. It never connects to or changes a pre-existing REAPER session or licence.
The isolated host scans the Windows VST3, opens its editor, renders eight seconds,
saves the project, reopens it in another REAPER process and verifies parameter
recall and a second render. WAV checks reject silent or severely clipped output.
Reports, host-rendered WAVs and diagnostic screenshots are uploaded as a separate
Actions artifact. These files are evidence only after that Windows job succeeds;
the Lua/PowerShell host test has not run in this Linux environment.

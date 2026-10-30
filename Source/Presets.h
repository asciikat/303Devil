#pragma once

// Factory sound presets, exposed to the host as programs. A preset only sets the sound
// parameters below; anything it doesn't list goes back to its default. Sequencer
// settings and patterns are left alone.

#include "ParamIDs.h"

#include <array>

namespace presets
{
inline constexpr const char* kSoundParams[] = {
    pid::tune, pid::cutoff, pid::resonance, pid::envMod, pid::decay, pid::accent, pid::wave, pid::volume,
    pid::acid, pid::overdrive, pid::filterFM, pid::tracking, pid::sweep, pid::slideTime, pid::ampDecay,
    pid::accentDecay, pid::softAttack, pid::muffler, pid::distortion, pid::sub,
    pid::delayMix, pid::delayTime, pid::delayFeedback, pid::reverbMix, pid::reverbSize, pid::pump,
    pid::lfoDepth, pid::lfoRate, pid::squelch, pid::tweak, pid::tweakRate,
};

// Values are in the parameter's own units: 0..1 for percentages, ms, dB, 0/1 for
// switches, and the choice index for Waveform (0 = saw, 1 = square),
// Delay Time (0 = 1/16, 1 = 1/8, 2 = 3/16, 3 = 1/4, 4 = 3/8) and
// Filter LFO Rate (0 = 1 beat ... 2 = 1 bar, 4 = 4 bars, 6 = 16 bars),
// Auto Tweak Rate (0 = 1 bar, 1 = 2 bars, 2 = 4 bars, 3 = 8 bars).
struct Value
{
    const char* id = nullptr;
    float value = 0.0f;
};

struct Preset
{
    const char* name;
    std::array<Value, 20> values;
};

inline constexpr Preset kFactory[] = {
    { "Init 303", {} },
    { "Classic Squelch",
      { { { pid::cutoff, 0.30f }, { pid::resonance, 0.85f }, { pid::envMod, 0.65f }, { pid::decay, 0.40f },
          { pid::accent, 0.80f } } } },
    { "Rubber Square",
      { { { pid::wave, 1.0f }, { pid::cutoff, 0.22f }, { pid::resonance, 0.45f }, { pid::envMod, 0.35f },
          { pid::decay, 0.25f }, { pid::accent, 0.50f } } } },
    { "Resonant Wow",
      { { { pid::cutoff, 0.20f }, { pid::resonance, 0.97f }, { pid::envMod, 0.80f }, { pid::decay, 0.60f },
          { pid::accent, 1.00f } } } },
    { "Deep Sub Line",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.15f }, { pid::resonance, 0.30f }, { pid::envMod, 0.25f },
          { pid::decay, 0.20f }, { pid::accent, 0.40f }, { pid::overdrive, 0.10f }, { pid::distortion, 0.0f },
          { pid::muffler, 0.20f }, { pid::sub, 0.70f } } } },
    { "Acid Screamer",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.30f }, { pid::resonance, 1.00f }, { pid::envMod, 0.85f },
          { pid::decay, 0.50f }, { pid::accent, 0.90f }, { pid::overdrive, 0.60f }, { pid::distortion, 0.70f },
          { pid::muffler, 0.30f }, { pid::sub, 0.40f }, { pid::slideTime, 90.0f } } } },
    { "Devil Fish Squelch",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.25f }, { pid::resonance, 0.90f }, { pid::envMod, 0.70f },
          { pid::filterFM, 0.20f }, { pid::tracking, 0.50f }, { pid::sweep, 0.70f }, { pid::accentDecay, 120.0f },
          { pid::ampDecay, 600.0f }, { pid::softAttack, 0.8f }, { pid::distortion, 0.20f }, { pid::sub, 0.30f } } } },
    { "Distorted Square",
      { { { pid::acid, 1.0f }, { pid::wave, 1.0f }, { pid::cutoff, 0.35f }, { pid::resonance, 0.70f },
          { pid::envMod, 0.60f }, { pid::decay, 0.35f }, { pid::overdrive, 0.40f }, { pid::distortion, 0.85f },
          { pid::sub, 0.50f } } } },
    { "Short Pluck",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.30f }, { pid::resonance, 0.60f }, { pid::envMod, 0.70f },
          { pid::decay, 0.10f }, { pid::ampDecay, 150.0f }, { pid::softAttack, 0.3f }, { pid::distortion, 0.25f } } } },
    { "Long Glide Dub",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.25f }, { pid::resonance, 0.75f }, { pid::envMod, 0.50f },
          { pid::decay, 0.70f }, { pid::slideTime, 250.0f }, { pid::distortion, 0.10f }, { pid::sub, 0.50f },
          { pid::delayMix, 0.35f }, { pid::delayTime, 2.0f }, { pid::delayFeedback, 0.6f }, { pid::reverbMix, 0.25f } } } },

    // Acid techno / main stage
    { "Main Stage Acid",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.32f }, { pid::resonance, 0.93f }, { pid::envMod, 0.78f },
          { pid::decay, 0.42f }, { pid::accent, 0.90f }, { pid::overdrive, 0.55f }, { pid::distortion, 0.55f },
          { pid::muffler, 0.30f }, { pid::sub, 0.40f }, { pid::delayMix, 0.22f }, { pid::delayTime, 2.0f },
          { pid::reverbMix, 0.18f }, { pid::reverbSize, 0.75f }, { pid::lfoDepth, 0.25f }, { pid::lfoRate, 4.0f } } } },
    { "Hard Acid Techno",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.28f }, { pid::resonance, 0.96f }, { pid::envMod, 0.82f },
          { pid::decay, 0.30f }, { pid::accent, 1.00f }, { pid::overdrive, 0.80f }, { pid::distortion, 0.90f },
          { pid::muffler, 0.40f }, { pid::sub, 0.45f }, { pid::accentDecay, 150.0f }, { pid::pump, 0.35f },
          { pid::delayMix, 0.12f }, { pid::delayTime, 1.0f } } } },
    { "Warehouse Squelch",
      { { { pid::cutoff, 0.25f }, { pid::resonance, 0.90f }, { pid::envMod, 0.70f }, { pid::decay, 0.55f },
          { pid::accent, 0.90f }, { pid::delayMix, 0.20f }, { pid::delayTime, 2.0f }, { pid::reverbMix, 0.30f },
          { pid::reverbSize, 0.85f } } } },
    { "Rave Acid Lead",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.40f }, { pid::resonance, 0.92f }, { pid::envMod, 0.75f },
          { pid::decay, 0.45f }, { pid::accent, 0.85f }, { pid::overdrive, 0.50f }, { pid::distortion, 0.50f },
          { pid::muffler, 0.30f }, { pid::sub, 0.20f }, { pid::slideTime, 80.0f }, { pid::delayMix, 0.30f },
          { pid::delayTime, 2.0f }, { pid::delayFeedback, 0.55f }, { pid::reverbMix, 0.25f } } } },
    { "Slow Sweep Hypno",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.22f }, { pid::resonance, 0.88f }, { pid::envMod, 0.55f },
          { pid::decay, 0.50f }, { pid::accent, 0.80f }, { pid::overdrive, 0.35f }, { pid::distortion, 0.30f },
          { pid::sub, 0.40f }, { pid::lfoDepth, 0.55f }, { pid::lfoRate, 6.0f }, { pid::delayMix, 0.18f },
          { pid::delayTime, 2.0f }, { pid::reverbMix, 0.15f } } } },
    { "Self-Osc Whistle",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.45f }, { pid::resonance, 1.00f }, { pid::envMod, 0.30f },
          { pid::decay, 0.60f }, { pid::accent, 0.70f }, { pid::tracking, 1.00f }, { pid::overdrive, 0.20f },
          { pid::distortion, 0.10f }, { pid::reverbMix, 0.20f } } } },
    { "Filter FM Growl",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.30f }, { pid::resonance, 0.85f }, { pid::envMod, 0.60f },
          { pid::decay, 0.35f }, { pid::filterFM, 0.50f }, { pid::overdrive, 0.50f }, { pid::distortion, 0.40f },
          { pid::sub, 0.30f } } } },
    { "Pumping Rubber",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.18f }, { pid::resonance, 0.55f }, { pid::envMod, 0.45f },
          { pid::decay, 0.30f }, { pid::sub, 0.60f }, { pid::muffler, 0.50f }, { pid::distortion, 0.15f },
          { pid::pump, 0.60f } } } },
    { "Minimal Bleep",
      { { { pid::acid, 1.0f }, { pid::wave, 1.0f }, { pid::cutoff, 0.50f }, { pid::resonance, 0.60f },
          { pid::envMod, 0.40f }, { pid::decay, 0.10f }, { pid::ampDecay, 120.0f }, { pid::softAttack, 0.3f },
          { pid::distortion, 0.0f }, { pid::sub, 0.20f }, { pid::delayMix, 0.30f }, { pid::delayTime, 2.0f },
          { pid::delayFeedback, 0.5f } } } },
    { "Wow Stack",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.20f }, { pid::resonance, 0.95f }, { pid::envMod, 0.65f },
          { pid::decay, 0.50f }, { pid::accent, 1.00f }, { pid::sweep, 0.15f }, { pid::accentDecay, 400.0f },
          { pid::overdrive, 0.40f }, { pid::distortion, 0.35f }, { pid::sub, 0.35f } } } },

    // Cheat presets: the auto tweak does the knob work for you
    { "CHEAT Auto Acid",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.28f }, { pid::resonance, 0.85f }, { pid::envMod, 0.60f },
          { pid::decay, 0.40f }, { pid::accent, 0.80f }, { pid::overdrive, 0.45f }, { pid::distortion, 0.45f },
          { pid::muffler, 0.30f }, { pid::sub, 0.40f }, { pid::delayMix, 0.20f }, { pid::delayTime, 2.0f },
          { pid::reverbMix, 0.15f }, { pid::squelch, 0.25f }, { pid::tweak, 0.65f }, { pid::tweakRate, 1.0f } } } },
    { "CHEAT Instant Rave",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.35f }, { pid::resonance, 0.90f }, { pid::envMod, 0.70f },
          { pid::decay, 0.45f }, { pid::accent, 0.90f }, { pid::overdrive, 0.60f }, { pid::distortion, 0.60f },
          { pid::sub, 0.40f }, { pid::delayMix, 0.28f }, { pid::delayTime, 2.0f }, { pid::reverbMix, 0.22f },
          { pid::pump, 0.35f }, { pid::squelch, 0.40f }, { pid::tweak, 0.80f }, { pid::tweakRate, 0.0f } } } },

    // Sounds for FOLLOW (set ROLE to match): a bass under the kick, and a quiet line on top
    { "FOLLOW Acid Bass",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.20f }, { pid::resonance, 0.72f }, { pid::envMod, 0.50f },
          { pid::decay, 0.28f }, { pid::accent, 0.75f }, { pid::overdrive, 0.30f }, { pid::distortion, 0.20f },
          { pid::muffler, 0.30f }, { pid::sub, 0.55f }, { pid::pump, 0.30f }, { pid::lfoDepth, 0.20f },
          { pid::lfoRate, 5.0f }, { pid::tweak, 0.30f }, { pid::tweakRate, 2.0f } } } },
    { "FOLLOW Gentle Overlay",
      { { { pid::acid, 1.0f }, { pid::cutoff, 0.34f }, { pid::resonance, 0.82f }, { pid::envMod, 0.50f },
          { pid::decay, 0.38f }, { pid::accent, 0.60f }, { pid::overdrive, 0.15f }, { pid::distortion, 0.10f },
          { pid::muffler, 0.40f }, { pid::sub, 0.0f }, { pid::slideTime, 90.0f }, { pid::delayMix, 0.30f },
          { pid::delayTime, 2.0f }, { pid::delayFeedback, 0.55f }, { pid::reverbMix, 0.28f }, { pid::reverbSize, 0.80f },
          { pid::lfoDepth, 0.35f }, { pid::lfoRate, 4.0f }, { pid::volume, -10.0f } } } },
};

inline constexpr int kNumFactory = (int) (sizeof (kFactory) / sizeof (kFactory[0]));
} // namespace presets

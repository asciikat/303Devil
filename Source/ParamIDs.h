#pragma once

// Parameter IDs. These are saved in projects and presets: don't rename them.
namespace pid
{
// Classic panel
inline constexpr const char* tune      = "tune";
inline constexpr const char* cutoff    = "cutoff";
inline constexpr const char* resonance = "reso";
inline constexpr const char* envMod    = "envmod";
inline constexpr const char* decay     = "decay";
inline constexpr const char* accent    = "accent";
inline constexpr const char* wave      = "wave";
inline constexpr const char* volume    = "volume";

// Acid mode
inline constexpr const char* acid        = "acid";
inline constexpr const char* overdrive   = "drive";
inline constexpr const char* filterFM    = "filterfm";
inline constexpr const char* tracking    = "tracking";
inline constexpr const char* sweep       = "sweep";
inline constexpr const char* slideTime   = "slide";
inline constexpr const char* ampDecay    = "ampdecay";
inline constexpr const char* accentDecay = "accdecay";
inline constexpr const char* softAttack  = "attack";
inline constexpr const char* muffler     = "muffler";
inline constexpr const char* distortion  = "dist";
inline constexpr const char* sub         = "sub";

// Main-stage FX
inline constexpr const char* delayMix      = "delay";
inline constexpr const char* delayTime     = "delaytime";
inline constexpr const char* delayFeedback = "delayfb";
inline constexpr const char* reverbMix     = "reverb";
inline constexpr const char* reverbSize    = "revsize";
inline constexpr const char* pump          = "pump";
inline constexpr const char* lfoDepth      = "lfodepth";
inline constexpr const char* lfoRate       = "lforate";

// Cheat panel
inline constexpr const char* squelch   = "squelch";
inline constexpr const char* tweak     = "tweak";
inline constexpr const char* tweakRate = "tweakrate";
inline constexpr const char* style     = "style";
inline constexpr const char* scale     = "scale";

// Ghost Hands
inline constexpr const char* ghostPerform   = "ghost";
inline constexpr const char* ghostAmount    = "ghostamt";
inline constexpr const char* ghostVariation = "ghostvar";
inline constexpr const char* ghostLength    = "ghostlen";
inline constexpr const char* ghostMode      = "ghostmode";

// Energy
inline constexpr const char* energy    = "energy";
inline constexpr const char* energyArc = "energyarc";

// Sequencer
inline constexpr const char* source    = "source";
inline constexpr const char* clock     = "clock";
inline constexpr const char* run       = "run";
inline constexpr const char* bpm       = "bpm";
inline constexpr const char* root      = "root";
inline constexpr const char* shuffle   = "shuffle";
inline constexpr const char* length    = "length";
inline constexpr const char* pattern   = "pattern";
inline constexpr const char* accentVel = "accvel";
inline constexpr const char* roll      = "roll";
inline constexpr const char* role      = "role";
} // namespace pid

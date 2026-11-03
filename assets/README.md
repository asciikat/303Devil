# Punk Fish artwork

- `knob_lime.png`, `knob_pink.png`, `knob_black.png`: original 3D assets rendered
  with Blender/Cycles using `tools/art/render_knobs.py`. Transparent, unmarked caps
  deliberately separate material lighting from the live JUCE value/ghost pointers.
- `orange_metal.png`: generated orange powder-coat material texture, with no baked
  controls or legends. It is drawn once into the editor background cache.

All artwork is embedded through `PunkFishArtwork` during the normal CMake build.
No Blender installation, image-generation service or external asset path is needed
by end users. No artwork or source was extracted from Lemus or Phoscyon.

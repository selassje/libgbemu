Vendored from Matt Currie's [cgb-acid2](https://github.com/mattcurrie/cgb-acid2)
(MIT License, see `LICENSE`), a CGB PPU rendering-accuracy test ROM - the CGB
companion to `dmg-acid2` (see `../dmg-acid2`), exercising CGB-only features
(background/object palettes, VRAM bank 1 tile attributes, BG-to-OAM
priority) that dmg-acid2 doesn't touch. Its header declares the cartridge as
CGB-required (0x0143 = 0xC0), not just CGB-aware.

- `cgb-acid2.gbc` - the prebuilt ROM from the
  [v1.1 release](https://github.com/mattcurrie/cgb-acid2/releases/tag/v1.1)
  (upstream only ships the RGBDS assembly source; building it ourselves would
  mean adding the RGBDS toolchain to every CI job just for this one ROM).
- `reference.rgb` - the official reference image (`img/reference.png`
  upstream), converted from PNG to a raw 160x144 RGB24 byte dump (row-major,
  no header) so the test can compare against it with a plain byte comparison
  instead of pulling in a PNG decoding dependency.

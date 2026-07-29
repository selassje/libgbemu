Vendored from Matt Currie's [dmg-acid2](https://github.com/mattcurrie/dmg-acid2)
(MIT License, see `LICENSE`), a DMG PPU rendering-accuracy test ROM.

- `dmg-acid2.gb` - the prebuilt ROM from the
  [v1.0 release](https://github.com/mattcurrie/dmg-acid2/releases/tag/v1.0)
  (upstream only ships the RGBDS assembly source; building it ourselves would
  mean adding the RGBDS toolchain to every CI job just for this one ROM).
- `reference.rgb` - the official reference image
  (`img/reference-dmg.png` upstream), converted from PNG to a raw
  160x144 RGB24 byte dump (row-major, no header) so the test can compare
  against it with a plain byte comparison instead of pulling in a PNG
  decoding dependency.

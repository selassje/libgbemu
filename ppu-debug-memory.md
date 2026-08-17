# PPU debugging notes

- `m3_lcdc_bg_en_change.gb` toggles LCDC bit 0 while LCDC bit 7 remains set.
- On DMG, LCDC bit 0 is applied during final background/object pixel mixing:
  the background fetcher and FIFO continue normally, but a popped background
  pixel is displayed as color index 0 while the bit is clear.
- Clearing LCDC bit 0 should not flush, stop, or discard pushes to the
  background FIFO.
- `LD (HL), r` performs its memory write on the final T-cycle.
- Mesen debugger `H` values describe instruction boundaries and do not expose
  the exact dot of the underlying `FF40` bus write.
- No documented LCDC bit-0 quirk has been found that would blank pixels before
  the register write.
- The captured libgbemu trap was:
  `LY=1, dot=108, x=16, scxLow=0`.
- The corresponding instruction was `0662: LD (HL), C`, with `C=$92`.
- The Mesen instruction boundary was two dots later (`H=103` versus
  libgbemu's `H=101`), which is insufficient by itself to explain a 16-pixel
  difference.
- Mesen's `$9800` tile map begins with `A B C D E F G ...`; its blank first
  output tile is not caused by a blank tile-map entry.

## Sprites are real, and matter for timing

- The ROM places 18 sprites (`sprite_data`), one per 8-line group covering
  the whole screen (`Y = $10, $18, ... $98`), all using tile index 0 (a
  blank/transparent tile - `LCDC=$93` has OBJ enable set). `X` increases by
  1 per group (`0, 1, 2, ... 17`).
- These sprites are invisible (blank tile - contribute zero visible
  pixels). Their only role is timing: fetching them stalls the pixel
  pipeline, which shifts *when* the fixed-timing LCDC-toggle interrupt's
  effect becomes visible relative to pixel output. The blank top-left tile
  itself still comes entirely from LCDC bit 0 (background/window enable)
  being clear at pop-time, per the earlier notes - not from bit 3, and not
  from the sprite's own pixels.
- Confirmed via the trap (`LY=1`, sprite X=0, fully off-screen): before any
  fixes, our object fetch was a **fixed dot-count, independent of the
  sprite's X** - measured identical blanked-region output across all 18
  X-groups. Real hardware's varies with `X mod 8` (clean +1-per-group slope
  visible in the reference image for X=8..15, the on-screen ones).
- Root cause, from gbdev Pan Docs `pixel_fifo.md` "Sprites" (pulled via
  `gh api repos/gbdev/pandocs/contents/src/pixel_fifo.md`, and confirmed by
  the user against the same source): the in-flight background/window fetch
  is **not** preempted the instant an object's X is reached - it keeps
  advancing one step per dot until it reaches step 5 (Push) or the
  background FIFO already has pixels, and only then does the object fetch
  begin. Only after that wait does a further fixed sequence run (advance
  two steps: 1 dot then 3 dots; retrieve lower address: 1 dot; upper
  address: 0 dots; render+advance: 1 dot if X != 160).

## `MODE_2_DOTS=50` red herring

- A debug `std::cerr` line without an explicit `std::dec` inherited
  hex-mode formatting left over from the opcode-trace output earlier in the
  stream. `0x50 = 80` decimal - `MODE_2_DOTS` was correct (80) the whole
  time; mode 3 genuinely starts at dot 80 as expected. Any future debug
  `std::cerr` line here should force `std::dec` explicitly (the existing
  `DMG BG-disabled FIFO pop` trap already does this correctly).

## Fix progress (verified via the `LY=1`/scanline-1 trap, X=0 sprite)

Each row is a real, rebuilt-and-measured data point, not a guess:

| Change | `x` (pixelsRendered) at the LCDC write |
| --- | --- |
| Original (baseline) | 23 |
| `elapsedDots >= 2` for background fetch steps (was `>= 1`) - matches Pan Docs' "first four steps take 2 dots each"; verified zero regressions across the full suite (85/86 pass, only this test red) in Release with `ctest -j16` | 16 |
| + `checkForObject()` wait-gate (don't switch to the object fetch until the in-flight fetch reaches `PushToFifo` or the background FIFO is non-empty) | 16 (no change alone - see below) |
| + object-mode `ReadTileDataLow`/`ReadTileDataHigh` bumped to `elapsedDots >= 3` (was `>= 2`, background stays `>= 2`) | 14 |

Notable dead ends, so they aren't retried blindly:

- The wait-gate **alone** does nothing to the final `x`, even though it
  demonstrably delays when the object fetch starts (dot 80 -> dot 87 for
  this case). Pixels keep draining from the FIFO at the same rate during
  the extra wait, so delaying the *start* of the stall doesn't reduce the
  total pixel count by a fixed later dot - only lengthening the fetch's own
  *duration* does that.
- Forcing the interrupted background/window fetch to restart from
  `State::ReadTile` after the object fetch completes (rather than resuming
  exactly where `saveFetcherState()`/`restoreFetcherState()` left it) made
  `x` *worse* (16, not better) in this scenario - by the time the wait-gated
  trigger actually fires (dot 87, second tile's fetch barely started
  either way), there's little real progress left to discard, so forcing a
  restart doesn't add the stall it seems like it should. Implemented as
  `Ppu::Fetcher::restartCurrentFetch()` (declared in `modules/ppu.cppm`,
  defined in `src/ppu.cpp` right before `reset()`) - method is currently
  defined but **not called** (reverted the call site, kept the method since
  it may still be useful once the real mechanism is understood).

Still need roughly 10-14 more pixels of stall to reach the reference's
near-column-0 blanking for this X=0 scanline. Pan Docs' fixed per-object
costs (5-6 dots beyond the wait) don't obviously add up to a gap that
large, so there's likely still a missing mechanism, not just a
miscounted constant. Object Fetch Canceling (LCDC.1 toggling mid-fetch)
is a documented but different trigger - doesn't apply here since LCDC.1
stays constant in this ROM; only LCDC.0 toggles.

All of the above changes (wait-gate, object-fetch `elapsedDots >= 3`,
`restartCurrentFetch()`, plus the `LY==1`-scoped `std::cerr` debug prints
in `checkForObject()`/`reset()`/the `ReadTileDataHigh` completion branch)
are uncommitted in the working tree as of this note - not yet cleaned up
or committed.

## Current status (2026-08-17)

### Background fetcher

- The first four fetcher steps use two dots each.
- `Get Tile Data High` also attempts to push the completed row.
- `Sleep` attempts another push on each of its two dots. This accounts for
  the documented three push opportunities: once at the end of Tile Data
  High and twice during Sleep.
- A row is pushed only while the background/window FIFO is empty.
- If the row has already been pushed by the end of Sleep, the fetcher goes
  directly to `Get Tile`; otherwise it enters the explicit Push retry state,
  which retries every dot until successful.
- This removed the cumulative horizontal drift seen with the older fetcher
  duration/state progression.

### LCD startup and visible-pixel timing

- LCD enable currently initializes the PPU dot counter to 7, following the
  Mesen startup convention investigated during this session.
- The startup scanline enters Drawing at dot 87 and starts the renderer at
  dot 92. The initial pipeline/dummy pixels are discarded before visible
  output; on the captured `m3_bgp_change` LY=0, visible x=0 is produced at
  libgbemu dot 97.
- Later visible scanlines use the normal Mode 3 path (Drawing at dot 84 and
  renderer start at dot 89 in the current local dot convention).
- STAT/OAM interrupt timing has separate startup handling: LY>0 can trigger
  at the line boundary, while LY=0 uses the startup dot-4 event. This made
  the first test-loop BGP schedule consistent between LY=0 and LY=1.

### CPU sub-cycle I/O timing

- `LD ($FF00+C),A` (`E2`) writes to BGP currently use uniform sub-cycle
  timing: hardware advances to one T-cycle before the instruction boundary,
  then the MMU write is committed. The remaining cycle is accounted for by
  the normal CPU timing machinery.
- For a write logged at PPU dot N, the pixel rendered on dot N already sees
  the new BGP value. Example from LY=0: the write at dot 171 is observed by
  visible pixel x=74 on dot 171.
- Do not restore the experimental `BGP value != 0` special case. It moved
  nonzero values one additional T-cycle earlier and made LY=0 pixel-perfect
  only by coincidence. On later rows the normal/restoration palette byte is
  itself nonzero, so the same rule moved the opposite edges early and left
  every later row with 3-4 incorrect boundary pixels.

### `m3_bgp_change` comparison

With the current uniform one-T-cycle-early BGP commit, LY=0 is:

| Output | Pixel runs `(first-last: shade)` |
| --- | --- |
| libgbemu | `0-1:white, 2-13:gray, 14-73:white, 74-85:dark, 86-145:white, 146-157:black, 158-159:white` |
| reference | `0:white, 1-13:gray, 14-72:white, 73-85:dark, 86-144:white, 145-157:black, 158-159:white` |

- Therefore the starts of the gray, dark-gray, and black spans are one pixel
  late, while all transitions back to white are aligned.
- The LY=0 BGP writes are at dots 83, 99, 111, 171, 183, 243, and 255.
  The corresponding visible transitions caused by writes at 99, 111, 171,
  183, 243, and 255 occur at x=2, 14, 74, 86, 146, and 158.
- The temporary nonzero-byte experiment changed LY=0 to an exact reference
  match, but comparison of all rows showed only LY=0 matching; LY=1-143 each
  retained 3-4 boundary errors. That experiment has been reverted.
- The full `m3_bgp_change` screenshot test still fails in the current state.
  A blanket one-dot shift for every BGP write is not a valid fix because it
  would correct the colored-span starts while making their already-correct
  ending transitions one pixel early.

### Debug instrumentation still present

- CPU logging records BGP writes and `E2` instruction start dots.
- PPU logging records the first eight LY=0 pixels with dot, x, background
  color index, BGP, and resolved shade.
- The earlier LCDC.0-disabled pop abort remains removed/commented, so test
  execution is not terminated by the diagnostic path.

export module gbemu:hard_assert;

import std;

namespace gbemu {

// A release-mode-active assertion for internal invariants that genuinely
// should be unreachable (as opposed to std::expected, this codebase's usual
// error-reporting path for conditions a caller can legitimately hit, like a
// malformed ROM or save state) - plain <cassert>/assert() compiles out under
// NDEBUG (every Release preset here defines it), which would silently turn
// a real logic bug into undefined behavior instead of a diagnosable crash.
// Deliberately not a thrown exception either: most call sites (e.g. Ppu's
// internal Fifo, or GameBoy::reset() re-deriving from a model+ROM pair its
// own callers already validated) have no try/catch anywhere up their call
// stack, so an uncaught exception there would just crash with no
// diagnostic. std::abort() is what a debugger/crash dump can actually point
// at the real failure site. Deliberately not marked `export` (same reason
// HardwareMode isn't - see its own comment): usable module-wide, but not
// part of gbemu's public API.
void
hardAssert(bool condition, // NOLINT(misc-use-internal-linkage)
           std::string_view message);

}

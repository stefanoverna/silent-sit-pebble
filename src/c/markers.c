#include "markers.h"

// See markers.h. This is the entire behavioural core of the app: a pure,
// side-effect-free function that is trivial to reason about by hand.
//
//   cycle = duration_min * 60          length of one cycle, in seconds
//   pos   = elapsed % cycle            position inside the current cycle
//   tick  = interval_min * 60          0 when the periodic tick is off
//
// is_end  : pos wrapped back to 0 (and we are past the very start) -> restart
// is_half : exactly half-way through the cycle (always integer seconds, since
//           durations are whole minutes -> half is a multiple of 30s)
// is_tick : on a periodic tick boundary
//
// Collision rule: the triple wins. If a periodic tick lands on the same second
// as the half or the end, the single tick is suppressed.
MarkerEvent marker_for_elapsed(int elapsed, SilentSitConfig cfg) {
  if (elapsed <= 0) return MARKER_NONE;

  int cycle = (int)cfg.duration_min * 60;
  if (cycle <= 0) return MARKER_NONE;

  int pos  = elapsed % cycle;
  int tick = (int)cfg.interval_min * 60;

  bool is_end  = (pos == 0);
  bool is_half = (pos == cycle / 2);
  bool is_tick = (tick > 0) && (elapsed % tick == 0);

  if (is_end)  return MARKER_END;    // end wins over a coincident tick
  if (is_half) return MARKER_HALF;   // half wins over a coincident tick
  if (is_tick) return MARKER_TICK;
  return MARKER_NONE;
}

// Smallest marker position strictly greater than `elapsed`. The candidates are
// the next end, the next half, and (if enabled) the next tick; the earliest of
// them is the answer. The event type at that instant is taken from the pure
// marker_for_elapsed(), so collision precedence is reused, not re-derived.
int next_marker_after(int elapsed, SilentSitConfig cfg, MarkerEvent *out_event) {
  int cycle = (int)cfg.duration_min * 60;
  if (cycle <= 0) { if (out_event) *out_event = MARKER_NONE; return -1; }

  int tick = (int)cfg.interval_min * 60;
  int half = cycle / 2;
  if (elapsed < 0) elapsed = 0;

  // Next end: next strictly-greater multiple of cycle.
  long next_end = ((long)(elapsed / cycle) + 1) * cycle;

  // Next half: positions half, half+cycle, half+2*cycle, … first one > elapsed.
  long k = (elapsed >= half) ? ((long)(elapsed - half) / cycle + 1) : 0;
  long next_half = half + k * cycle;

  long best = (next_half < next_end) ? next_half : next_end;

  if (tick > 0) {
    long next_tick = ((long)(elapsed / tick) + 1) * tick;
    if (next_tick < best) best = next_tick;
  }

  if (out_event) *out_event = marker_for_elapsed((int)best, cfg);
  return (int)best;
}

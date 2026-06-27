#include "session_window.h"
#include "app.h"
#include "markers.h"
#include "locale.h"

// The RUNNING screen. One window, three internal states:
//
//   RUNNING  -> shows "N min", counting up forever through cycles.
//   CONFIRM  -> BACK was pressed once; a second BACK stops, anything else (or a
//               timeout) returns to RUNNING. The seduta keeps running meanwhile.
//   SUMMARY  -> the seduta has stopped; shows the total. BACK returns to setup.
//
// The display never resets across cycle loops; the cycle markers do (handled by
// markers.c). Timekeeping is anchored to absolute time(NULL) for zero drift.
//
// Battery-friendly scheduling (scoping §4, "approach B"): instead of waking
// every second, we keep two sparse timers —
//   * a MINUTE_UNIT tick that refreshes the "N min" readout, and
//   * a single app_timer armed to fire at exactly the next marker; on fire it
//     vibrates and re-arms for the following marker.
// Each re-arm recomputes the delay from absolute elapsed time, so a late timer
// self-corrects and nothing accumulates drift.

typedef enum { S_RUNNING, S_CONFIRM, S_SUMMARY } SessionState;

static Window    *s_window;
static TextLayer *s_time_layer;     // very large elapsed-minutes number
static TextLayer *s_unit_layer;     // the word "minuti" under the number
static TextLayer *s_status_layer;   // small line: Quiet Time / confirm prompt
static GFont      s_time_font;       // custom LECO font for the minutes number

static SessionState s_state;
static time_t  s_start_time;        // time(NULL) at start — the drift anchor
static int     s_last_shown_min;    // last minute drawn (redraw only on change)
static bool    s_quiet_active;      // Quiet Time on at start -> show indicator
static AppTimer *s_confirm_timer;   // auto-cancels the CONFIRM prompt
static AppTimer *s_marker_timer;    // fires at the next cycle marker
static MarkerEvent s_next_event;    // the marker s_marker_timer will deliver
static Animation *s_summary_anim;   // the "breath" played when SUMMARY appears

static char s_time_buf[16];         // backs s_time_layer (must outlive the call)

#define CONFIRM_TIMEOUT_MS 4000

// --- Vibration patterns (calibrated in slice 3: light but perceptible) -------

// Scheme B ("heavy end"): three distinct textures. The tick is a brief tap; the
// half is a medium double; the end is two long "gong" pulses so the cycle
// boundary is felt as heavier, never confused with the half. Intensity is fixed
// by hardware — only rhythm (pulse/gap durations) distinguishes them.
//
// The user's chosen strength (g_config.vibe_strength) lengthens the buzzes:
// stronger levels stretch the "on" pulses (even-indexed segments) while leaving
// the silent gaps untouched, so each marker keeps its recognizable rhythm and
// only feels more emphatic. Percentages are relative to the base "light" timing.

static const uint16_t VIBE_SCALE_PCT[VIBE_STRENGTH_COUNT] = {
  [VIBE_LIGHT]  = 100,
  [VIBE_MEDIUM] = 175,
  [VIBE_STRONG] = 260,
};

#define VIBE_MAX_SEGMENTS 3

// Scale the on-pulses of `base` by `strength` and enqueue. The scaled pattern is
// copied by vibes_enqueue_custom_pattern, so a stack buffer is fine.
static void vibe_play(const uint32_t *base, uint32_t num_segments, uint8_t strength) {
  if (strength >= VIBE_STRENGTH_COUNT) strength = VIBE_LIGHT;
  uint16_t pct = VIBE_SCALE_PCT[strength];

  uint32_t seg[VIBE_MAX_SEGMENTS];
  for (uint32_t i = 0; i < num_segments && i < VIBE_MAX_SEGMENTS; i++) {
    seg[i] = (i % 2 == 0) ? base[i] * pct / 100 : base[i];   // scale pulses, keep gaps
  }
  vibes_enqueue_custom_pattern((VibePattern){ .durations = seg, .num_segments = num_segments });
}

static void vibe_tick(void) {
  static const uint32_t seg[] = {70};   // brief tap
  vibe_play(seg, 1, g_config.vibe_strength);
}

static void vibe_half(void) {
  static const uint32_t seg[] = {110, 140, 110};   // medium double
  vibe_play(seg, 3, g_config.vibe_strength);
}

static void vibe_end(void) {
  static const uint32_t seg[] = {300, 150, 300};   // two long "gong" pulses
  vibe_play(seg, 3, g_config.vibe_strength);
}

// Settings preview: a single tap at the chosen strength (the tick texture is the
// most familiar, so the level difference reads clearly).
void vibe_preview(uint8_t strength) {
  static const uint32_t seg[] = {70};
  vibe_play(seg, 1, strength);
}

// --- Display ------------------------------------------------------------------

static void show_status(const char *text) {
  text_layer_set_text(s_status_layer, text);
}

static void update_minutes(int elapsed) {
  int min = elapsed / 60;
  if (min != s_last_shown_min) {
    s_last_shown_min = min;
    snprintf(s_time_buf, sizeof(s_time_buf), "%d", min);   // number only
    text_layer_set_text(s_time_layer, s_time_buf);
  }
}

static void fire_marker(MarkerEvent ev, int elapsed) {
  switch (ev) {
    case MARKER_END:
      APP_LOG(APP_LOG_LEVEL_DEBUG, "marker END @ %ds", elapsed);
      vibe_end();
      break;
    case MARKER_HALF:
      APP_LOG(APP_LOG_LEVEL_DEBUG, "marker HALF @ %ds", elapsed);
      vibe_half();
      break;
    case MARKER_TICK:
      APP_LOG(APP_LOG_LEVEL_DEBUG, "marker TICK @ %ds", elapsed);
      vibe_tick();
      break;
    case MARKER_NONE:
      break;
  }
}

// --- Marker scheduler (app_timer to the next event) ---------------------------

static void marker_timer_cb(void *data);

static void schedule_next_marker(void) {
  int elapsed = (int)(time(NULL) - s_start_time);
  if (elapsed < 0) elapsed = 0;

  int when = next_marker_after(elapsed, g_config, &s_next_event);
  if (when < 0) return;   // no cycle (shouldn't happen: duration is always > 0)

  int delay_ms = (when - elapsed) * 1000;
  if (delay_ms < 1) delay_ms = 1;
  s_marker_timer = app_timer_register((uint32_t)delay_ms, marker_timer_cb, NULL);
}

static void marker_timer_cb(void *data) {
  s_marker_timer = NULL;
  int elapsed = (int)(time(NULL) - s_start_time);
  fire_marker(s_next_event, elapsed);
  update_minutes(elapsed);     // keep the readout fresh right after a marker
  schedule_next_marker();      // re-arm from absolute time -> self-correcting
}

// --- Summary "breath" (an expanding ripple behind the number) -----------------
// When the seduta ends, a single circle blooms outward from the centre and
// dissipates — one slow exhale, rendered behind the total. Pebble has no fill
// alpha, so the fade is faked by stepping the stroke colour toward the black
// background (greys on colour models; on 1-bit the ring just thins out and
// stops). s_breath_pct holds animation progress, or -1 when nothing should draw.

static Layer *s_circle_layer;
static int    s_breath_pct = -1;

static void circle_update_proc(Layer *layer, GContext *ctx) {
  if (s_breath_pct < 0) return;

  GRect b = layer_get_bounds(layer);
  GPoint center = grect_center_point(&b);

  // Grow from the centre to just past the far corner so the ring fully clears.
  int max_r  = (b.size.w + b.size.h) / 2;
  int radius = (max_r * s_breath_pct) / 100;
  if (radius < 1) return;

#ifdef PBL_COLOR
  // Fade the stroke from a soft grey toward black as the ring expands.
  uint8_t lum = (uint8_t)((220 * (100 - s_breath_pct)) / 100);
  graphics_context_set_stroke_color(ctx, GColorFromRGB(lum, lum, lum));
#else
  // 1-bit: no greys to fade through — draw only the first ~70% so the ring
  // appears to dissipate before it reaches the edges.
  if (s_breath_pct > 70) return;
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  // Stroke thickens as the ring expands, so the bloom feels like it swells.
  graphics_context_set_stroke_width(ctx, (uint8_t)(1 + (16 * s_breath_pct) / 100));
  graphics_draw_circle(ctx, center, radius);
}

static void breath_update(Animation *anim, const AnimationProgress progress) {
  s_breath_pct = (int)(((int32_t)progress * 100) / ANIMATION_NORMALIZED_MAX);
  if (s_circle_layer) layer_mark_dirty(s_circle_layer);
}

static void breath_teardown(Animation *anim) {
  s_breath_pct = -1;                 // stop drawing once the bloom is done
  if (s_circle_layer) layer_mark_dirty(s_circle_layer);
}

static void breath_stopped(Animation *anim, bool finished, void *ctx) {
  s_summary_anim = NULL;             // framework auto-destroys the animation
}

static const AnimationImplementation s_breath_impl = {
  .update   = breath_update,
  .teardown = breath_teardown,
};

static void play_summary_breath(void) {
#ifdef SCREENSHOT_FAKE_ELAPSED
  // Store screenshots are stills: freeze the ripple at a representative point in
  // its bloom instead of racing the live animation against capture latency.
  // Guarded by the same screenshot-only macro as the faked elapsed time, so a
  // normal build always plays the full animation.
  s_breath_pct = 40;
  if (s_circle_layer) layer_mark_dirty(s_circle_layer);
  return;
#endif
  s_breath_pct = -1;
  s_summary_anim = animation_create();
  animation_set_implementation(s_summary_anim, &s_breath_impl);
  animation_set_curve(s_summary_anim, AnimationCurveEaseOut);
  animation_set_duration(s_summary_anim, 3200);
  animation_set_handlers(s_summary_anim,
                         (AnimationHandlers){ .stopped = breath_stopped }, NULL);
  animation_schedule(s_summary_anim);
}

// --- Stop / summary -----------------------------------------------------------

static void enter_summary(void) {
  tick_timer_service_unsubscribe();
  if (s_marker_timer)  { app_timer_cancel(s_marker_timer);  s_marker_timer = NULL; }
  if (s_confirm_timer) { app_timer_cancel(s_confirm_timer); s_confirm_timer = NULL; }

  int total_min = (int)(time(NULL) - s_start_time) / 60;
  s_state = S_SUMMARY;

  s_last_shown_min = -1;  // force the layer to accept the summary text
  snprintf(s_time_buf, sizeof(s_time_buf), "%d", total_min);   // number only
  text_layer_set_text(s_time_layer, s_time_buf);
  show_status(L(MSG_SESSION_ENDED));

  play_summary_breath();   // a single ripple blooms behind the total
}

static void confirm_timeout(void *data) {
  s_confirm_timer = NULL;
  if (s_state == S_CONFIRM) {
    s_state = S_RUNNING;
    show_status(s_quiet_active ? L(MSG_QUIET_ACTIVE) : "");
  }
}

// --- Per-minute display tick (cheap: one wake per minute) ---------------------

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  int elapsed = (int)(time(NULL) - s_start_time);
  if (elapsed < 0) elapsed = 0;
  update_minutes(elapsed);
}

// --- Buttons ------------------------------------------------------------------

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  switch (s_state) {
    case S_RUNNING:
      s_state = S_CONFIRM;
      show_status(L(MSG_CONFIRM_STOP));
      s_confirm_timer = app_timer_register(CONFIRM_TIMEOUT_MS, confirm_timeout, NULL);
      break;
    case S_CONFIRM:
      enter_summary();
      break;
    case S_SUMMARY:
      window_stack_pop(true);   // back to the setup screen
      break;
  }
}

static void select_handler(ClickRecognizerRef recognizer, void *context) {
  // In CONFIRM, SELECT means "no, keep going".
  if (s_state == S_CONFIRM) {
    if (s_confirm_timer) { app_timer_cancel(s_confirm_timer); s_confirm_timer = NULL; }
    s_state = S_RUNNING;
    show_status(s_quiet_active ? L(MSG_QUIET_ACTIVE) : "");
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_handler);
}

// --- Window lifecycle ---------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Background ripple, added first so it draws behind every text layer. Idle
  // (draws nothing) until the seduta ends and play_summary_breath() runs.
  s_breath_pct = -1;
  s_circle_layer = layer_create(bounds);
  layer_set_update_proc(s_circle_layer, circle_update_proc);
  layer_add_child(root, s_circle_layer);

  // Huge elapsed-minutes number, with the word "minuti" right below it. The
  // number uses the custom LECO font (digits-only subset) at 72px.
  //
  // That font renders with ~18px of blank space above the digit ink and a
  // baseline ~68px below the layer top (visible cap height ~50px). The time
  // layer must stay at the full 86px line height so glyphs never clip — but if
  // we put "minuti" directly under that tall box it floats far below the number.
  // So we anchor the label near the number's baseline instead, and center the
  // *visible* block (digit ink + label) on screen rather than the layer boxes.
  const int num_h    = 87;   // digit line-height box (>= 86 so nothing clips)
  const int unit_h   = 30;
  const int ink_top  = 18;   // blank space above the digit ink within num_h
  const int baseline = 68;   // digit baseline offset within num_h
  const int unit_off = baseline + 6;   // sit the label just below the baseline

  // Visible block: from the digit ink top down to the label's caps (~23px).
  int block_h   = (unit_off + 23) - ink_top;
  int group_top = (bounds.size.h - block_h) / 2 - ink_top;

  s_time_layer = text_layer_create(GRect(0, group_top, bounds.size.w, num_h));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LECO_REGULAR_SUBSET_72));
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  s_unit_layer = text_layer_create(GRect(0, group_top + unit_off, bounds.size.w, unit_h));
  text_layer_set_background_color(s_unit_layer, GColorClear);
  text_layer_set_text_color(s_unit_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  text_layer_set_font(s_unit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_unit_layer, GTextAlignmentCenter);
  text_layer_set_text(s_unit_layer, L(MSG_UNIT_MINUTES));
  layer_add_child(root, text_layer_get_layer(s_unit_layer));

  // Small status line near the bottom.
  s_status_layer = text_layer_create(
      GRect(0, bounds.size.h - 30, bounds.size.w, 24));
  text_layer_set_background_color(s_status_layer, GColorClear);
  text_layer_set_text_color(s_status_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_status_layer));

  // Fresh seduta: anchor the timer to now and reset state.
  s_state = S_RUNNING;
  s_start_time = time(NULL);
#ifdef SCREENSHOT_FAKE_ELAPSED
  // Screenshot builds only: pretend the seduta began SCREENSHOT_FAKE_ELAPSED
  // seconds ago so store captures show a realistic, non-zero readout instead of
  // "0". Compiled out of every normal build — the macro is defined solely by the
  // env-driven CFLAG in wscript. See tools/generate-screenshots.sh.
  s_start_time -= SCREENSHOT_FAKE_ELAPSED;
#endif
  s_last_shown_min = -1;
  s_quiet_active = quiet_time_is_active();

  update_minutes((int)(time(NULL) - s_start_time));   // show elapsed immediately
  show_status(s_quiet_active ? L(MSG_QUIET_ACTIVE) : "");

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);  // display only
  schedule_next_marker();                                   // arm first marker
}

static void window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  if (s_marker_timer)  { app_timer_cancel(s_marker_timer);  s_marker_timer = NULL; }
  if (s_confirm_timer) { app_timer_cancel(s_confirm_timer); s_confirm_timer = NULL; }
  // Stop the ripple (fires teardown synchronously) before its layer is freed.
  if (s_summary_anim) { animation_unschedule(s_summary_anim); s_summary_anim = NULL; }
  layer_destroy(s_circle_layer);
  s_circle_layer = NULL;
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_unit_layer);
  text_layer_destroy(s_status_layer);
  fonts_unload_custom_font(s_time_font);
}

void session_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
    });
    window_set_click_config_provider(s_window, click_config_provider);
  }
  window_stack_push(s_window, true);
}

void session_window_deinit(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

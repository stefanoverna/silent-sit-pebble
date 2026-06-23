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

static SessionState s_state;
static time_t  s_start_time;        // time(NULL) at start — the drift anchor
static int     s_last_shown_min;    // last minute drawn (redraw only on change)
static bool    s_quiet_active;      // Quiet Time on at start -> show indicator
static AppTimer *s_confirm_timer;   // auto-cancels the CONFIRM prompt
static AppTimer *s_marker_timer;    // fires at the next cycle marker
static MarkerEvent s_next_event;    // the marker s_marker_timer will deliver

static char s_time_buf[16];         // backs s_time_layer (must outlive the call)

#define CONFIRM_TIMEOUT_MS 4000

// --- Vibration patterns (calibrated in slice 3: light but perceptible) -------

// Scheme B ("heavy end"): three distinct textures. The tick is a brief tap; the
// half is a medium double; the end is two long "gong" pulses so the cycle
// boundary is felt as heavier, never confused with the half. Intensity is fixed
// by hardware — only rhythm (pulse/gap durations) distinguishes them.

static void vibe_tick(void) {
  static const uint32_t seg[] = {70};   // brief tap
  vibes_enqueue_custom_pattern((VibePattern){ .durations = seg, .num_segments = 1 });
}

static void vibe_half(void) {
  static const uint32_t seg[] = {110, 140, 110};   // medium double
  vibes_enqueue_custom_pattern((VibePattern){ .durations = seg, .num_segments = 3 });
}

static void vibe_end(void) {
  static const uint32_t seg[] = {300, 150, 300};   // two long "gong" pulses
  vibes_enqueue_custom_pattern((VibePattern){ .durations = seg, .num_segments = 3 });
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

  // Huge elapsed-minutes number, with the word "minuti" right below it. The
  // number uses the largest system numeric font (digits only); the pair is
  // centred as a group in the upper two-thirds (status line sits at the bottom).
  const int num_h  = 58;
  const int unit_h = 30;
  int group_top = (bounds.size.h - (num_h + unit_h)) / 2 - 10;

  s_time_layer = text_layer_create(GRect(0, group_top, bounds.size.w, num_h));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  s_unit_layer = text_layer_create(GRect(0, group_top + num_h, bounds.size.w, unit_h));
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
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_unit_layer);
  text_layer_destroy(s_status_layer);
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

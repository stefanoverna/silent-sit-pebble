#include <pebble.h>
#include "app.h"
#include "setup_window.h"
#include "settings_window.h"
#include "session_window.h"
#include "quiet_window.h"
#include "locale.h"

// Silent Sit — a silent, tactile meditation timer for Pebble.
//
// main.c owns the config and the navigation router. The screens live in:
//   setup_window.c    home screen (title / Inizia / config summary)
//   settings_window.c the durata / intervallo menu (reached with Down)
//   quiet_window.c    "turn on Quiet Time" reminder
//   session_window.c  the running seduta + confirm-stop + summary
//   markers.c         the pure cycle/tick/half/end scheduler

SilentSitConfig g_config;

// Lifetime meditation total, in seconds. Loaded at startup, bumped whenever a
// seduta ends, and persisted on each change so it survives app close / reboot.
static uint32_t s_total_seconds;

static void config_load(void) {
  g_config.duration_min = persist_exists(PKEY_DURATION)
      ? (uint8_t)persist_read_int(PKEY_DURATION) : DEFAULT_DURATION;
  g_config.interval_min = persist_exists(PKEY_INTERVAL)
      ? (uint8_t)persist_read_int(PKEY_INTERVAL) : DEFAULT_INTERVAL;
  g_config.vibe_strength = persist_exists(PKEY_STRENGTH)
      ? (uint8_t)persist_read_int(PKEY_STRENGTH) : DEFAULT_STRENGTH;
  s_total_seconds = persist_exists(PKEY_TOTAL_SECS)
      ? (uint32_t)persist_read_int(PKEY_TOTAL_SECS) : 0;
}

void meditation_add_seconds(uint32_t seconds) {
  s_total_seconds += seconds;
  persist_write_int(PKEY_TOTAL_SECS, (int32_t)s_total_seconds);
}

uint32_t meditation_total_seconds(void) {
  return s_total_seconds;
}

void meditation_reset(void) {
  s_total_seconds = 0;
  persist_write_int(PKEY_TOTAL_SECS, 0);
}

// Split the lifetime total into whole hours + tenths of an hour, rounded to the
// nearest tenth. Shared by both total formatters so they always agree.
static void total_hours_tenths(int *h, int *tenths) {
  int mins = (int)(s_total_seconds / 60);
  *h = mins / 60;
  *tenths = ((mins % 60) * 10 + 30) / 60;   // round to nearest 0.1 h
  if (*tenths >= 10) { (*h)++; *tenths = 0; }   // carry e.g. 1.95 h -> 2.0 h
}

void meditation_format_total(char *buf, size_t size) {
  int mins = (int)(s_total_seconds / 60);
  if (mins < 60) {
    snprintf(buf, size, L(MSG_TOTAL_MIN), mins);   // "Hai meditato per 43 min"
    return;
  }
  // An hour or more: show decimal hours ("12,5 ore"), rounded to a tenth.
  int h, tenths;
  total_hours_tenths(&h, &tenths);
  char num[12];
  snprintf(num, sizeof(num), "%d%s%d", h, locale_decimal_sep(), tenths);
  snprintf(buf, size, L(MSG_TOTAL_HOURS), num);   // "Hai meditato per 12,5 ore"
}

void meditation_format_total_short(char *buf, size_t size) {
  int mins = (int)(s_total_seconds / 60);
  if (mins < 60) {
    snprintf(buf, size, "%d min", mins);   // "43 min" (min is a universal token)
    return;
  }
  int h, tenths;
  total_hours_tenths(&h, &tenths);
  snprintf(buf, size, "%d%s%d h", h, locale_decimal_sep(), tenths);   // "12,5 h"
}

void config_save(void) {
  persist_write_int(PKEY_DURATION, g_config.duration_min);
  persist_write_int(PKEY_INTERVAL, g_config.interval_min);
  persist_write_int(PKEY_STRENGTH, g_config.vibe_strength);
}

// Router: from setup's "Inizia". If Quiet Time is already on, start straight
// away; otherwise show the reminder (which can still proceed).
void start_session_flow(void) {
  if (quiet_time_is_active()) {
    session_window_push();
  } else {
    quiet_window_push();
  }
}

static void init(void) {
  locale_init();   // pick the language before any window draws strings
  config_load();
  setup_window_push();
}

static void deinit(void) {
  config_save();
  session_window_deinit();
  quiet_window_deinit();
  settings_window_deinit();
  setup_window_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

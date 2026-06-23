#include <pebble.h>
#include "app.h"
#include "setup_window.h"
#include "session_window.h"
#include "quiet_window.h"
#include "locale.h"

// Vipassana — a silent, tactile meditation timer for Pebble.
//
// main.c owns the config and the navigation router. The screens live in:
//   setup_window.c    pre-seduta config (durata / intervallo / Inizia)
//   quiet_window.c    "turn on Quiet Time" reminder
//   session_window.c  the running seduta + confirm-stop + summary
//   markers.c         the pure cycle/tick/half/end scheduler

VipassanaConfig g_config;

static void config_load(void) {
  g_config.duration_min = persist_exists(PKEY_DURATION)
      ? (uint8_t)persist_read_int(PKEY_DURATION) : DEFAULT_DURATION;
  g_config.interval_min = persist_exists(PKEY_INTERVAL)
      ? (uint8_t)persist_read_int(PKEY_INTERVAL) : DEFAULT_INTERVAL;
}

void config_save(void) {
  persist_write_int(PKEY_DURATION, g_config.duration_min);
  persist_write_int(PKEY_INTERVAL, g_config.interval_min);
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
  setup_window_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

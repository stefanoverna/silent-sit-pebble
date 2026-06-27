#pragma once

// Tiny string-table localization. The system language is read once at startup
// (i18n_get_system_locale()) and every user-facing string is looked up by id.
// English is the default/fallback; Italian is selected when the system locale
// starts with "it". Universal tokens ("min", "tick", "off", the button name
// "Select", and the Pebble feature name "Quiet Time") are left as literals at
// their call sites — only language-dependent strings live here.

typedef enum {
  MSG_UNIT_MINUTES,    // "minutes" — unit under the elapsed number
  MSG_QUIET_ACTIVE,    // status line when Quiet Time is on
  MSG_CONFIRM_STOP,    // status line after BACK once
  MSG_SESSION_ENDED,   // summary status line
  MSG_START,           // home screen action, incl. the noun ("Inizia seduta")
  MSG_HOME_SUMMARY,    // home config line, fmt(duration, interval): "30 min, a tick every 10 min"
  MSG_HOME_SUMMARY_OFF,// home config line when ticks are off, fmt(duration): "30 min, no ticks"
  MSG_DURATION,        // settings row: expected session length
  MSG_TICK_INTERVAL,   // settings row: tick interval
  MSG_VIBE_STRENGTH,   // settings row: vibration strength
  MSG_VIBE_LIGHT,      // strength value: light
  MSG_VIBE_MEDIUM,     // strength value: medium
  MSG_VIBE_STRONG,     // strength value: strong
  MSG_RESET_TOTAL,     // settings row: wipe the lifetime tally
  MSG_RESET_CONFIRM,   // settings row subtitle once armed: "Press again"
  MSG_TOTAL_HOURS,     // lifetime tally, %s hours (decimal): "Hai meditato per 12,5 ore"
  MSG_TOTAL_MIN,       // lifetime tally, %d minutes: "Hai meditato per 43 min"
  MSG_QUIET_TITLE,     // Quiet-Time reminder headline ("Turn on Quiet Time")
  MSG_QUIET_BODY,      // Quiet-Time reminder body (the reason)
  MSG_QUIET_HINT,      // Quiet-Time reminder hint
  MSG_COUNT
} MsgId;

// Detect the system language. Call once at startup, before any window loads.
void locale_init(void);

// The localized string for `id`. Always returns a stable, static pointer safe
// to hand to text_layer_set_text(); falls back to English on any miss.
const char *L(MsgId id);

// The decimal mark for the active language ("." for English, "," elsewhere).
const char *locale_decimal_sep(void);

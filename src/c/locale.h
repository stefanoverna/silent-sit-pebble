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
  MSG_START,           // setup row 0 title
  MSG_DURATION,        // setup row 1 title (expected session length)
  MSG_TICK_INTERVAL,   // setup row 2 title
  MSG_QUIET_BODY,      // Quiet-Time reminder body
  MSG_QUIET_HINT,      // Quiet-Time reminder hint
  MSG_COUNT
} MsgId;

// Detect the system language. Call once at startup, before any window loads.
void locale_init(void);

// The localized string for `id`. Always returns a stable, static pointer safe
// to hand to text_layer_set_text(); falls back to English on any miss.
const char *L(MsgId id);

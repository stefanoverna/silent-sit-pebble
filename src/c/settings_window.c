#include "settings_window.h"
#include "app.h"
#include "session_window.h"
#include "locale.h"

// The settings menu: a 4-row MenuLayer reached with DOWN from the home screen.
//
//   Row 0  Durata       "30 min"          -> cycles the cycle length
//   Row 1  Intervallo   "10 min" / "off"  -> cycles the tick interval
//   Row 2  Vibrazione   "Leggero"/...     -> cycles the buzz strength
//   Row 3  Azzera tot.  "12,5 h"          -> wipes the lifetime tally (2 taps)
//
// SELECT on a value row cycles it (with wrap) and persists immediately. The
// reset row is destructive, so it arms on the first SELECT (subtitle becomes
// "Press again") and only wipes on the second; moving off the row or leaving the
// menu disarms it. BACK pops back to the home screen, refreshing its summary.

static const uint8_t DURATIONS[] = {10, 15, 20, 30, 45, 60};
static const uint8_t INTERVALS[] = {10, 30, 0};   // 0 = off
static const uint8_t STRENGTHS[] = {VIBE_LIGHT, VIBE_MEDIUM, VIBE_STRONG};
#define N_DURATIONS (sizeof(DURATIONS) / sizeof(DURATIONS[0]))
#define N_INTERVALS (sizeof(INTERVALS) / sizeof(INTERVALS[0]))
#define N_STRENGTHS (sizeof(STRENGTHS) / sizeof(STRENGTHS[0]))

#define ROW_DURATION 0
#define ROW_INTERVAL 1
#define ROW_STRENGTH 2
#define ROW_RESET    3
#define NUM_ROWS     4

static Window    *s_window;
static MenuLayer *s_menu;

// Two-tap guard for the destructive reset row: the first SELECT arms it, the
// second wipes. Cleared whenever the selection leaves the row or the menu opens.
static bool s_reset_armed;

// Advance `value` to the next entry in `table` (wrapping). If the current value
// isn't found, fall back to the first entry.
static uint8_t cycle_value(const uint8_t *table, size_t n, uint8_t value) {
  for (size_t i = 0; i < n; i++) {
    if (table[i] == value) return table[(i + 1) % n];
  }
  return table[0];
}

static void format_interval(uint8_t interval, char *buf, size_t size) {
  if (interval == 0) snprintf(buf, size, "off");
  else               snprintf(buf, size, "%d min", interval);
}

// The localized label for a vibration strength (falls back to "light").
static const char *strength_label(uint8_t strength) {
  switch (strength) {
    case VIBE_MEDIUM: return L(MSG_VIBE_MEDIUM);
    case VIBE_STRONG: return L(MSG_VIBE_STRONG);
    default:          return L(MSG_VIBE_LIGHT);
  }
}

// --- Menu callbacks -----------------------------------------------------------

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *data) {
  return NUM_ROWS;
}

// Buffers are filled and drawn within the same call, so locals are fine here:
// menu_cell_basic_draw renders immediately rather than storing the pointer.
static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  char sub[24];
  switch (idx->row) {
    case ROW_DURATION:
      snprintf(sub, sizeof(sub), "%d min", g_config.duration_min);
      menu_cell_basic_draw(ctx, cell, L(MSG_DURATION), sub, NULL);
      break;
    case ROW_INTERVAL:
      format_interval(g_config.interval_min, sub, sizeof(sub));
      menu_cell_basic_draw(ctx, cell, L(MSG_TICK_INTERVAL), sub, NULL);
      break;
    case ROW_STRENGTH:
      menu_cell_basic_draw(ctx, cell, L(MSG_VIBE_STRENGTH),
                           strength_label(g_config.vibe_strength), NULL);
      break;
    case ROW_RESET:
      if (s_reset_armed) {
        menu_cell_basic_draw(ctx, cell, L(MSG_RESET_TOTAL), L(MSG_RESET_CONFIRM), NULL);
      } else {
        meditation_format_total_short(sub, sizeof(sub));   // "12,5 h" / "43 min"
        menu_cell_basic_draw(ctx, cell, L(MSG_RESET_TOTAL), sub, NULL);
      }
      break;
  }
}

static void select_click(MenuLayer *ml, MenuIndex *idx, void *data) {
  // The reset row stands apart: it doesn't touch g_config, and it takes two
  // taps. Handle it here and return before the config_save() the value rows need.
  if (idx->row == ROW_RESET) {
    if (s_reset_armed) {
      meditation_reset();
      s_reset_armed = false;
      vibe_preview(VIBE_LIGHT);   // a short buzz confirms the wipe happened
    } else {
      s_reset_armed = true;       // arm; the subtitle now reads "Press again"
    }
    menu_layer_reload_data(s_menu);
    return;
  }

  switch (idx->row) {
    case ROW_DURATION:
      g_config.duration_min = cycle_value(DURATIONS, N_DURATIONS, g_config.duration_min);
      break;
    case ROW_INTERVAL:
      g_config.interval_min = cycle_value(INTERVALS, N_INTERVALS, g_config.interval_min);
      break;
    case ROW_STRENGTH:
      g_config.vibe_strength = cycle_value(STRENGTHS, N_STRENGTHS, g_config.vibe_strength);
      vibe_preview(g_config.vibe_strength);   // buzz so the new strength is felt
      break;
  }
  config_save();
  menu_layer_reload_data(s_menu);
}

// Disarm the reset row the moment the selection moves off it, so an armed
// "Press again" can never be confirmed from a different row or linger on screen.
static void selection_changed(MenuLayer *ml, MenuIndex new_index,
                              MenuIndex old_index, void *data) {
  if (s_reset_armed && new_index.row != ROW_RESET) {
    s_reset_armed = false;
    menu_layer_reload_data(s_menu);
  }
}

// --- Window lifecycle ---------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows      = get_num_rows,
    .draw_row          = draw_row,
    .select_click      = select_click,
    .selection_changed = selection_changed,
  });
  menu_layer_set_normal_colors(s_menu, GColorBlack, GColorWhite);
  menu_layer_set_highlight_colors(s_menu,
      PBL_IF_COLOR_ELSE(GColorIndigo, GColorWhite),
      PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
}

void settings_window_push(void) {
  s_reset_armed = false;   // every fresh entry starts disarmed
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void settings_window_deinit(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

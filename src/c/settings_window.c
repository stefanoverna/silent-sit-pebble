#include "settings_window.h"
#include "app.h"
#include "locale.h"

// The settings menu: a 2-row MenuLayer reached with DOWN from the home screen.
//
//   Row 0  Durata       "30 min"          -> cycles the cycle length
//   Row 1  Intervallo   "10 min" / "off"  -> cycles the tick interval
//
// SELECT on a row cycles its value (with wrap) and persists it immediately.
// BACK pops back to the home screen, which refreshes its summary on appear.

static const uint8_t DURATIONS[] = {10, 15, 20, 30, 45, 60};
static const uint8_t INTERVALS[] = {10, 30, 0};   // 0 = off
#define N_DURATIONS (sizeof(DURATIONS) / sizeof(DURATIONS[0]))
#define N_INTERVALS (sizeof(INTERVALS) / sizeof(INTERVALS[0]))

#define ROW_DURATION 0
#define ROW_INTERVAL 1
#define NUM_ROWS     2

static Window    *s_window;
static MenuLayer *s_menu;

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
  }
}

static void select_click(MenuLayer *ml, MenuIndex *idx, void *data) {
  switch (idx->row) {
    case ROW_DURATION:
      g_config.duration_min = cycle_value(DURATIONS, N_DURATIONS, g_config.duration_min);
      break;
    case ROW_INTERVAL:
      g_config.interval_min = cycle_value(INTERVALS, N_INTERVALS, g_config.interval_min);
      break;
  }
  config_save();
  menu_layer_reload_data(s_menu);
}

// --- Window lifecycle ---------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .draw_row     = draw_row,
    .select_click = select_click,
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

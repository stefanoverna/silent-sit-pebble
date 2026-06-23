#include "setup_window.h"
#include "settings_window.h"
#include "app.h"
#include "locale.h"

// The home screen — the app's root window.
//
//   Silent Sit                (small title, top)
//
//      ▶ Inizia               (▶ glyph + action verb, centred as one unit)
//   30 min, with a tick        (current config, smaller, wraps to two lines)
//      every 10 min
//
//        v                    (caret, bottom — "press Down for settings")
//
// SELECT starts a seduta with the current config. DOWN opens the settings menu
// (durata / intervallo); on the way back the summary line re-reads the config.

static Window     *s_window;
static TextLayer  *s_title_layer;
static TextLayer  *s_action_layer;
static Layer      *s_action_icon_layer;   // ▶ play glyph, left of the action verb
static GDrawCommandImage *s_action_image;  // PDC vector (white)
static TextLayer  *s_summary_layer;
static Layer      *s_caret_layer;
static GDrawCommandImage *s_caret_image;   // PDC vector (white)

// PDC glyphs draw at their layer's origin; colours are baked into the vector.
static void action_icon_update(Layer *layer, GContext *ctx) {
  if (s_action_image) gdraw_command_image_draw(ctx, s_action_image, GPointZero);
}
static void caret_update(Layer *layer, GContext *ctx) {
  if (s_caret_image) gdraw_command_image_draw(ctx, s_caret_image, GPointZero);
}

// Holds the natural-language summary, e.g. "30 min session, with a tick every
// 10 min" — wide enough for the longest localized phrasing.
static char s_summary[64];

#define ACTION_H 34   // "Inizia" line height (GOTHIC_28_BOLD)
#define GROUP_GAP 4   // space between the action verb and the summary
#define SUMMARY_INSET 8   // horizontal breathing room so the summary wraps
#define ICON_GAP 6    // space between the ▶ glyph and the action verb
#define ICON_VOFF 3   // nudge the glyph down to sit on the text's optical centre

// Re-measure the summary and re-centre the whole block. The action row centres
// the ▶ glyph + verb as one unit; the summary (one or two lines) sits below, and
// the action+summary group is vertically centred so the layout stays balanced.
static void relayout(void) {
  if (!s_window || !s_action_layer || !s_summary_layer || !s_action_image) return;
  GRect bounds = layer_get_bounds(window_get_root_layer(s_window));

  GFont act_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont sum_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  int act_w = graphics_text_layout_get_content_size(L(MSG_START), act_font,
      GRect(0, 0, bounds.size.w, ACTION_H), GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
  int sum_w = bounds.size.w - 2 * SUMMARY_INSET;
  int sum_h = graphics_text_layout_get_content_size(s_summary, sum_font,
      GRect(0, 0, sum_w, 1000), GTextOverflowModeWordWrap, GTextAlignmentCenter).h + 2;

  GSize icon = gdraw_command_image_get_bounds_size(s_action_image);
  int group_h   = ACTION_H + GROUP_GAP + sum_h;
  int group_top = (bounds.size.h - group_h) / 2;

  // Centre the icon+verb unit horizontally.
  int row_w = icon.w + ICON_GAP + act_w;
  int row_l = (bounds.size.w - row_w) / 2;
  layer_set_frame(s_action_icon_layer,
      GRect(row_l, group_top + (ACTION_H - icon.h) / 2 + ICON_VOFF,
            icon.w, icon.h));
  layer_set_frame(text_layer_get_layer(s_action_layer),
      GRect(row_l + icon.w + ICON_GAP, group_top, act_w + 4, ACTION_H));

  layer_set_frame(text_layer_get_layer(s_summary_layer),
      GRect(SUMMARY_INSET, group_top + ACTION_H + GROUP_GAP, sum_w, sum_h));
}

// Rebuild the config summary from the live g_config. Called on every appear so
// returning from the settings menu reflects any change immediately.
static void refresh_summary(void) {
  if (g_config.interval_min == 0) {
    snprintf(s_summary, sizeof(s_summary), L(MSG_HOME_SUMMARY_OFF),
             g_config.duration_min);
  } else {
    snprintf(s_summary, sizeof(s_summary), L(MSG_HOME_SUMMARY),
             g_config.duration_min, g_config.interval_min);
  }
  if (s_summary_layer) {
    text_layer_set_text(s_summary_layer, s_summary);
    relayout();
  }
}

// --- Buttons ------------------------------------------------------------------

static void select_handler(ClickRecognizerRef recognizer, void *context) {
  start_session_flow();
}

static void down_handler(ClickRecognizerRef recognizer, void *context) {
  settings_window_push();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_handler);
}

// --- Window lifecycle ---------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Small title, pinned to the top.
  s_title_layer = text_layer_create(GRect(0, 6, bounds.size.w, 24));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_text(s_title_layer, "Silent Sit");
  layer_add_child(root, text_layer_get_layer(s_title_layer));

  // Centred block: the action verb with the config summary beneath it. Frames
  // here are placeholders — refresh_summary()/relayout() measure the (possibly
  // two-line) summary and centre the block once its text is known.
  s_action_image = gdraw_command_image_create_with_resource(RESOURCE_ID_ACTION_START);
  GSize action_sz = gdraw_command_image_get_bounds_size(s_action_image);
  s_action_icon_layer = layer_create(GRect(0, 0, action_sz.w, action_sz.h));
  layer_set_update_proc(s_action_icon_layer, action_icon_update);
  layer_add_child(root, s_action_icon_layer);

  s_action_layer = text_layer_create(GRect(0, 0, bounds.size.w, ACTION_H));
  text_layer_set_background_color(s_action_layer, GColorClear);
  text_layer_set_text_color(s_action_layer, GColorWhite);
  text_layer_set_font(s_action_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_action_layer, GTextAlignmentLeft);
  text_layer_set_text(s_action_layer, L(MSG_START));
  layer_add_child(root, text_layer_get_layer(s_action_layer));

  s_summary_layer = text_layer_create(GRect(0, 0, bounds.size.w, 1));
  text_layer_set_background_color(s_summary_layer, GColorClear);
  text_layer_set_text_color(s_summary_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  text_layer_set_font(s_summary_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_summary_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_summary_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_summary_layer));
  refresh_summary();

  // Caret at the bottom, hinting that Down leads somewhere.
  s_caret_image = gdraw_command_image_create_with_resource(RESOURCE_ID_CARET_DOWN);
  GSize caret = gdraw_command_image_get_bounds_size(s_caret_image);
  s_caret_layer = layer_create(GRect(
      (bounds.size.w - caret.w) / 2, bounds.size.h - caret.h - 8,
      caret.w, caret.h));
  layer_set_update_proc(s_caret_layer, caret_update);
  layer_add_child(root, s_caret_layer);
}

static void window_appear(Window *window) {
  refresh_summary();   // reflect any change made in the settings menu
}

static void window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_action_layer);
  layer_destroy(s_action_icon_layer);
  gdraw_command_image_destroy(s_action_image);
  text_layer_destroy(s_summary_layer);
  layer_destroy(s_caret_layer);
  gdraw_command_image_destroy(s_caret_image);
  s_summary_layer = NULL;
  s_action_image = NULL;
}

void setup_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .appear = window_appear,
      .unload = window_unload,
    });
    window_set_click_config_provider(s_window, click_config_provider);
  }
  window_stack_push(s_window, true);
}

void setup_window_deinit(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

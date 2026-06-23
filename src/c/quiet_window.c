#include "quiet_window.h"
#include "session_window.h"
#include "locale.h"

// The Quiet Time reminder, shown only when Quiet Time is OFF at start.
//
// Pebble's Quiet Time is read-only — no app can toggle it — so we just nudge
// the user, then let them proceed. SELECT = "Inizia comunque"; BACK returns to
// setup. When the user proceeds we pop this window first, so the stack becomes
// [setup, session] and BACK from the seduta lands back on setup.
//
// Visuals: the muted-bell Quiet Time glyph sits as a hero above the title, and
// an ActionBarLayer puts the "start" icon on the SELECT button — the native
// Pebble way to say "press here to begin", replacing the old text hint.

static Window         *s_window;
static TextLayer      *s_title_layer;
static TextLayer      *s_body_layer;
static BitmapLayer    *s_hero_layer;
static ActionBarLayer *s_action_bar;
static GBitmap        *s_hero_bitmap;    // muted-bell Quiet Time glyph
static GBitmap        *s_start_icon;     // action-bar SELECT icon

static void select_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop(false);    // remove this reminder
  session_window_push();      // start the seduta on top of setup
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_handler);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Action bar on the right: the "start" icon on SELECT communicates "begin".
  s_start_icon = gbitmap_create_with_resource(RESOURCE_ID_ACTION_START);
  s_action_bar = action_bar_layer_create();
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_start_icon);
  action_bar_layer_set_background_color(s_action_bar,
      PBL_IF_COLOR_ELSE(GColorIndigo, GColorBlack));
  action_bar_layer_set_click_config_provider(s_action_bar, click_config_provider);
  action_bar_layer_add_to_window(s_action_bar, window);

  // Content lives left of the action bar.
  const int content_w = bounds.size.w - ACTION_BAR_WIDTH;

  // Hero glyph (muted bell) centred at the top of the content area.
  s_hero_bitmap = gbitmap_create_with_resource(RESOURCE_ID_QUIET_TIME);
  GRect hero = gbitmap_get_bounds(s_hero_bitmap);
  const int hero_top = 14;
  s_hero_layer = bitmap_layer_create(
      GRect((content_w - hero.size.w) / 2, hero_top, hero.size.w, hero.size.h));
  bitmap_layer_set_background_color(s_hero_layer, GColorClear);
  bitmap_layer_set_compositing_mode(s_hero_layer, GCompOpSet);   // honour alpha
  layer_add_child(root, bitmap_layer_get_layer(s_hero_layer));

  // Title + body, stacked below the hero.
  const int title_h = 30;
  const int gap     = 6;
  const int body_h  = 80;
  int title_top = hero_top + hero.size.h + 8;
  int body_top  = title_top + title_h + gap;

  s_title_layer = text_layer_create(GRect(4, title_top, content_w - 8, title_h));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, GColorWhite);
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_text(s_title_layer, "Quiet Time");
  layer_add_child(root, text_layer_get_layer(s_title_layer));

  s_body_layer = text_layer_create(GRect(6, body_top, content_w - 12, body_h));
  text_layer_set_background_color(s_body_layer, GColorClear);
  text_layer_set_text_color(s_body_layer, GColorWhite);
  text_layer_set_font(s_body_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_body_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_body_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(s_body_layer, L(MSG_QUIET_BODY));
  layer_add_child(root, text_layer_get_layer(s_body_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_body_layer);
  bitmap_layer_destroy(s_hero_layer);
  action_bar_layer_destroy(s_action_bar);
  gbitmap_destroy(s_hero_bitmap);
  gbitmap_destroy(s_start_icon);
}

void quiet_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
    });
    // Click config is owned by the ActionBarLayer (set in window_load).
  }
  window_stack_push(s_window, true);
}

void quiet_window_deinit(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

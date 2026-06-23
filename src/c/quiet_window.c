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
// Visuals: the muted-bell Quiet Time glyph is centred directly above the
// "Quiet Time" title.

static Window     *s_window;
static TextLayer  *s_title_layer;
static TextLayer  *s_body_layer;
static TextLayer  *s_hint_layer;
static Layer      *s_hero_layer;
static GDrawCommandImage *s_hero_image;   // muted-bell Quiet Time glyph (PDC vector)

// Draw the PDC hero glyph at the layer's origin; its colours are baked into the
// vector (white fill, black outline — both read on the indigo background).
static void hero_update(Layer *layer, GContext *ctx) {
  if (s_hero_image) gdraw_command_image_draw(ctx, s_hero_image, GPointZero);
}

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

  // Horizontal breathing room for the body: a wider inset wraps it into a
  // calmer, narrower column instead of running edge-to-edge.
  const int h_margin = bounds.size.w / 5;
  const int title_w  = bounds.size.w - 8;
  const int body_w   = bounds.size.w - 2 * h_margin;

  s_hero_image = gdraw_command_image_create_with_resource(RESOURCE_ID_QUIET_TIME);
  GSize hero = gdraw_command_image_get_bounds_size(s_hero_image);

  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont body_font  = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  const char *title_text = L(MSG_QUIET_TITLE);
  const char *body_text  = L(MSG_QUIET_BODY);

  // Measure the actual wrapped height of each string so the block is truly
  // centred — no dead space from fixed-height boxes, regardless of language.
  int title_h = graphics_text_layout_get_content_size(title_text, title_font,
      GRect(0, 0, title_w, 1000), GTextOverflowModeWordWrap, GTextAlignmentCenter).h + 2;
  int body_h = graphics_text_layout_get_content_size(body_text, body_font,
      GRect(0, 0, body_w, 1000), GTextOverflowModeWordWrap, GTextAlignmentCenter).h + 2;

  // Vertical group: [hero] gap [title] gap [body], centred in the space above
  // the bottom hint line.
  const int gap_hero  = 12;
  const int gap_title = 4;
  const int group_h   = hero.h + gap_hero + title_h + gap_title + body_h;
  const int avail_h   = bounds.size.h - 40;   // 40px reserved for the hint line
  int hero_top = (avail_h - group_h) / 2;
  if (hero_top < 6) hero_top = 6;
  int title_top = hero_top + hero.h + gap_hero;
  int body_top  = title_top + title_h + gap_title;

  // Hero glyph, horizontally centred above the title.
  s_hero_layer = layer_create(
      GRect((bounds.size.w - hero.w) / 2, hero_top, hero.w, hero.h));
  layer_set_update_proc(s_hero_layer, hero_update);
  layer_add_child(root, s_hero_layer);

  s_title_layer = text_layer_create(GRect(4, title_top, title_w, title_h));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, GColorWhite);
  text_layer_set_font(s_title_layer, title_font);
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_title_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(s_title_layer, title_text);
  layer_add_child(root, text_layer_get_layer(s_title_layer));

  s_body_layer = text_layer_create(GRect(h_margin, body_top, body_w, body_h));
  text_layer_set_background_color(s_body_layer, GColorClear);
  text_layer_set_text_color(s_body_layer, GColorWhite);
  text_layer_set_font(s_body_layer, body_font);
  text_layer_set_text_alignment(s_body_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_body_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(s_body_layer, body_text);
  layer_add_child(root, text_layer_get_layer(s_body_layer));

  s_hint_layer = text_layer_create(GRect(4, bounds.size.h - 34, bounds.size.w - 8, 28));
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  text_layer_set_text(s_hint_layer, L(MSG_QUIET_HINT));
  layer_add_child(root, text_layer_get_layer(s_hint_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_body_layer);
  text_layer_destroy(s_hint_layer);
  layer_destroy(s_hero_layer);
  gdraw_command_image_destroy(s_hero_image);
}

void quiet_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, PBL_IF_COLOR_ELSE(GColorIndigo, GColorBlack));
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
    });
    window_set_click_config_provider(s_window, click_config_provider);
  }
  window_stack_push(s_window, true);
}

void quiet_window_deinit(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

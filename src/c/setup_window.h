#pragma once
#include <pebble.h>

// Push the home screen (the app's root window): title, Start action, config
// summary, and a Down-caret to the settings menu.
void setup_window_push(void);

// Free the window itself. Called once from the app's deinit().
void setup_window_deinit(void);

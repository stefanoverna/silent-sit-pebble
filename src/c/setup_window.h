#pragma once
#include <pebble.h>

// Push the pre-seduta setup screen (the app's root window).
void setup_window_push(void);

// Free the window itself. Called once from the app's deinit().
void setup_window_deinit(void);

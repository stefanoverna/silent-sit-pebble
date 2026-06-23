#pragma once
#include <pebble.h>

// Show the "turn on Quiet Time" reminder (used when Quiet Time is off at start).
void quiet_window_push(void);

// Free the window itself. Called once from the app's deinit().
void quiet_window_deinit(void);

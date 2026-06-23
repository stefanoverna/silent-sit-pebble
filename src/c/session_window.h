#pragma once
#include <pebble.h>

// Push the RUNNING screen and start a fresh seduta (resets the timer to 0).
void session_window_push(void);

// Free the window itself. Called once from the app's deinit().
void session_window_deinit(void);

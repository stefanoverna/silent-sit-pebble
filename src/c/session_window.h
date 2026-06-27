#pragma once
#include <pebble.h>

// Play a representative buzz at `strength` (VIBE_LIGHT|MEDIUM|STRONG) so the
// user feels the level while choosing it in settings. Reuses the marker engine.
void vibe_preview(uint8_t strength);

// Push the RUNNING screen and start a fresh seduta (resets the timer to 0).
void session_window_push(void);

// Free the window itself. Called once from the app's deinit().
void session_window_deinit(void);

#pragma once

// Push the settings menu (durata / intervallo), reached with DOWN from the home
// screen. BACK returns to the home screen, which re-reads the updated config.
void settings_window_push(void);

// Free the cached window on app exit.
void settings_window_deinit(void);

#pragma once
#include <pebble.h>
#include "markers.h"

// Persist keys (persistent storage survives app close and watch reboot).
#define PKEY_DURATION 1
#define PKEY_INTERVAL 2
#define PKEY_STRENGTH 3
#define PKEY_TOTAL_SECS 4   // lifetime meditation time, accumulated across sedute

#define DEFAULT_DURATION 30        // minutes
#define DEFAULT_INTERVAL 10        // minutes
#define DEFAULT_STRENGTH VIBE_MEDIUM

// The single source of truth for the session config, owned by main.c.
extern SilentSitConfig g_config;

// Persist g_config to storage. Called on every change in the setup screen.
void config_save(void);

// Router (implemented in main.c). Called from the setup screen when the user
// chooses "Inizia": checks Quiet Time and either shows the reminder or starts.
void start_session_flow(void);

// Lifetime meditation tally (implemented in main.c). A finished seduta adds its
// elapsed seconds; the home screen reads the running total for its title swap.
void     meditation_add_seconds(uint32_t seconds);
uint32_t meditation_total_seconds(void);

// Render the lifetime total into `buf` as "Xh YYm" past an hour, else "Ym".
// Shared by the home title swap and the summary screen so both read the same.
void     meditation_format_total(char *buf, size_t size);

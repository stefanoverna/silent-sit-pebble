#pragma once
#include <pebble.h>
#include "markers.h"

// Persist keys (persistent storage survives app close and watch reboot).
#define PKEY_DURATION 1
#define PKEY_INTERVAL 2

#define DEFAULT_DURATION 30   // minutes
#define DEFAULT_INTERVAL 10   // minutes

// The single source of truth for the session config, owned by main.c.
extern VipassanaConfig g_config;

// Persist g_config to storage. Called on every change in the setup screen.
void config_save(void);

// Router (implemented in main.c). Called from the setup screen when the user
// chooses "Inizia": checks Quiet Time and either shows the reminder or starts.
void start_session_flow(void);

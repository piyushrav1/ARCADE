#pragma once

#include "platform.h"
#include "utils.h"

// Unified tabbed Settings UI (AUDIO | GRAPHICS | THEME | CONTROLS).
// context: -1 = Hub, 0 = Pong Arena, 1 = Neon Breaker.
// Returns true when user exits Settings.
bool update_and_draw_settings_ui(Input* input, int context,
                                 u32 bg_outer, u32 panel_bg, u32 border, u32 card_bg,
                                 u32 text, u32 text_dim, u32 accent);

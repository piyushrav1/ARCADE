#pragma once

#include "platform.h"
#include "utils.h"

// Shared Graphics Settings UI. Returns true when user exits the menu.
bool update_and_draw_graphics_ui(Input* input,
                                 u32 bg_outer, u32 panel_bg, u32 border, u32 card_bg,
                                 u32 text, u32 text_dim, u32 accent);

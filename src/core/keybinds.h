#pragma once

#include <Windows.h>
#include "platform.h"
#include "utils.h"

struct Pong_Binds {
	int p1_up;
	int p1_down;
	int p2_up;
	int p2_down;
	int pause;
	bool mouse_1p;
};

struct Breaker_Binds {
	int left;
	int right;
	int action;
	int pause;
	bool mouse_aim;
};

extern Pong_Binds pong_binds;
extern Breaker_Binds breaker_binds;

inline bool vk_down(const Input* in, int vk) {
	return (vk > 0 && vk < 256) ? in->keys_down[vk] : false;
}

inline bool vk_pressed(const Input* in, int vk) {
	return (vk > 0 && vk < 256) ? (in->keys_down[vk] && in->keys_changed[vk]) : false;
}

void reset_pong_binds();
void reset_breaker_binds();
const char* vk_name(int vk);

// Shared Controls / Keybind UI. Returns true when user exits the menu.
// game_tab: 0 = Pong Arena, 1 = Neon Breaker.
bool update_and_draw_keybinds_ui(Input* input, int* game_tab, bool allow_tab_switch,
                                 u32 bg_outer, u32 panel_bg, u32 border, u32 card_bg,
                                 u32 text, u32 text_dim, u32 accent);


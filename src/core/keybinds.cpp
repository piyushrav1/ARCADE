#include "keybinds.h"
#include "renderer.h"

Pong_Binds pong_binds = { 'W', 'S', VK_UP, VK_DOWN, VK_ESCAPE, false };
Breaker_Binds breaker_binds = { 'A', 'D', VK_SPACE, VK_ESCAPE, true };

void
reset_pong_binds() {
	pong_binds = { 'W', 'S', VK_UP, VK_DOWN, VK_ESCAPE, false };
}

void
reset_breaker_binds() {
	breaker_binds = { 'A', 'D', VK_SPACE, VK_ESCAPE, true };
}

const char*
vk_name(int vk) {
	static char single[2] = { 0, 0 };
	if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
		single[0] = (char)vk;
		return single;
	}
	switch (vk) {
		case VK_LBUTTON: return "LMB";
		case VK_RBUTTON: return "RMB";
		case VK_SPACE:   return "SPACE";
		case VK_RETURN:  return "ENTER";
		case VK_ESCAPE:  return "ESC";
		case VK_SHIFT:   return "SHIFT";
		case VK_LSHIFT:  return "LSHIFT";
		case VK_RSHIFT:  return "RSHIFT";
		case VK_CONTROL: return "CTRL";
		case VK_LCONTROL:return "LCTRL";
		case VK_RCONTROL:return "RCTRL";
		case VK_MENU:    return "ALT";
		case VK_LMENU:   return "LALT";
		case VK_RMENU:   return "RALT";
		case VK_TAB:     return "TAB";
		case VK_BACK:    return "BACK";
		case VK_UP:      return "UP";
		case VK_DOWN:    return "DOWN";
		case VK_LEFT:    return "LEFT";
		case VK_RIGHT:   return "RIGHT";
		case VK_F1:      return "F1";
		case VK_F2:      return "F2";
		case VK_F3:      return "F3";
		default:         return "KEY";
	}
}

global_variable int kb_row = 0;
global_variable bool kb_listening = false;

internal bool
in_rect(float px, float py, float cx, float cy, float hw, float hh) {
	return absf(px - cx) <= hw && absf(py - cy) <= hh;
}

bool
update_and_draw_keybinds_ui(Input* input, int* game_tab, bool allow_tab_switch,
                            u32 bg_outer, u32 panel_bg, u32 border, u32 card_bg,
                            u32 text, u32 text_dim, u32 accent) {
	int tab = *game_tab;
	const int num_binds = (tab == 0) ? 5 : 4;
	const int total_rows = num_binds + 3; // binds + mouse toggle + reset + back

	const char* labels[6];
	int* bind_ptrs[6] = {};
	if (tab == 0) {
		labels[0] = "P1 MOVE UP";   bind_ptrs[0] = &pong_binds.p1_up;
		labels[1] = "P1 MOVE DOWN"; bind_ptrs[1] = &pong_binds.p1_down;
		labels[2] = "P2 MOVE UP";   bind_ptrs[2] = &pong_binds.p2_up;
		labels[3] = "P2 MOVE DOWN"; bind_ptrs[3] = &pong_binds.p2_down;
		labels[4] = "PAUSE";        bind_ptrs[4] = &pong_binds.pause;
	} else {
		labels[0] = "MOVE LEFT";      bind_ptrs[0] = &breaker_binds.left;
		labels[1] = "MOVE RIGHT";     bind_ptrs[1] = &breaker_binds.right;
		labels[2] = "LAUNCH / LASER"; bind_ptrs[2] = &breaker_binds.action;
		labels[3] = "PAUSE";          bind_ptrs[3] = &breaker_binds.pause;
	}

	if (kb_row >= total_rows) kb_row = total_rows - 1;

	if (kb_listening) {
		int k = input->last_key_pressed;
		if (k != 0) {
			if (k == VK_ESCAPE) {
				kb_listening = false;
				play_tone(320, 40, 0.14f);
			} else if (kb_row < num_binds) {
				*bind_ptrs[kb_row] = k;
				kb_listening = false;
				play_tone(660, 55, 0.18f);
			}
		}
	} else {
		if (vk_pressed(input, VK_ESCAPE)) {
			kb_listening = false;
			kb_row = 0;
			play_tone(400, 40, 0.14f);
			return true;
		}
		if (allow_tab_switch && (vk_pressed(input, VK_LEFT) || vk_pressed(input, VK_RIGHT))) {
			*game_tab = 1 - tab;
			kb_row = 0;
			play_tone(520, 35, 0.15f);
			return false;
		}
		if (vk_pressed(input, VK_UP) || vk_pressed(input, 'W')) {
			kb_row = (kb_row + total_rows - 1) % total_rows;
			play_tone(440, 30, 0.12f);
		}
		if (vk_pressed(input, VK_DOWN) || vk_pressed(input, 'S')) {
			kb_row = (kb_row + 1) % total_rows;
			play_tone(440, 30, 0.12f);
		}

		float mx = input->mouse_x;
		float my = input->mouse_y;

		if (allow_tab_switch && vk_pressed(input, VK_LBUTTON)) {
			if (in_rect(mx, my, -24.f, 26.5f, 20.f, 2.8f)) { *game_tab = 0; kb_row = 0; play_tone(520, 35, 0.15f); return false; }
			if (in_rect(mx, my,  24.f, 26.5f, 20.f, 2.8f)) { *game_tab = 1; kb_row = 0; play_tone(520, 35, 0.15f); return false; }
		}

		bool activate = vk_pressed(input, VK_RETURN) || vk_pressed(input, VK_SPACE);
		for (int i = 0; i < total_rows; i++) {
			float ry = 17.5f - i * 7.2f;
			if (in_rect(mx, my, 0.f, ry, 66.f, 3.0f)) {
				if (input->mouse_moved && kb_row != i) {
					kb_row = i;
					play_tone(440, 24, 0.10f);
				}
				if (vk_pressed(input, VK_LBUTTON)) {
					kb_row = i;
					activate = true;
				}
			}
		}

		if (activate) {
			if (kb_row < num_binds) {
				kb_listening = true;
				play_tone(580, 40, 0.15f);
			} else if (kb_row == num_binds) {
				if (tab == 0) pong_binds.mouse_1p = !pong_binds.mouse_1p;
				else          breaker_binds.mouse_aim = !breaker_binds.mouse_aim;
				play_tone(520, 40, 0.15f);
			} else if (kb_row == num_binds + 1) {
				if (tab == 0) reset_pong_binds();
				else          reset_breaker_binds();
				play_tone(620, 60, 0.18f);
			} else {
				kb_listening = false;
				kb_row = 0;
				play_tone(400, 40, 0.14f);
				return true;
			}
		}
	}

	// Render Controls UI
	clear_screen(bg_outer);
	draw_rect(0.f, 0.f, 86.2f, 46.2f, border);
	draw_rect(0.f, 0.f, 85.0f, 45.0f, panel_bg);

	draw_text_centered("CONTROLS & KEYBINDS", 0.f, 36.f, 0.80f, accent);

	if (allow_tab_switch) {
		draw_rect(-24.f, 26.5f, 20.4f, 2.8f, tab == 0 ? accent : border);
		draw_rect(-24.f, 26.5f, 20.0f, 2.4f, card_bg);
		draw_text_centered("PONG ARENA", -24.f, 26.5f, 0.34f, tab == 0 ? text : text_dim);

		draw_rect(24.f, 26.5f, 20.4f, 2.8f, tab == 1 ? accent : border);
		draw_rect(24.f, 26.5f, 20.0f, 2.4f, card_bg);
		draw_text_centered("NEON BREAKER", 24.f, 26.5f, 0.34f, tab == 1 ? text : text_dim);
	} else {
		draw_text_centered(tab == 0 ? "PONG ARENA" : "NEON BREAKER", 0.f, 26.5f, 0.42f, text_dim);
	}

	for (int i = 0; i < total_rows; i++) {
		float ry = 17.5f - i * 7.2f;
		bool sel = (i == kb_row);
		bool listening_row = (sel && kb_listening);

		draw_rect(0.f, ry, 66.4f, 3.0f, listening_row ? 0xFACC15 : (sel ? accent : border));
		draw_rect(0.f, ry, 66.0f, 2.6f, card_bg);

		if (i < num_binds) {
			draw_text_left(labels[i], -60.f, ry, 0.36f, sel ? text : text_dim);
			const char* kstr = listening_row ? "PRESS ANY KEY..." : vk_name(*bind_ptrs[i]);
			u32 kcol = listening_row ? 0xFACC15 : accent;
			draw_text_centered(kstr, 40.f, ry, 0.36f, kcol);
		} else if (i == num_binds) {
			bool m_on = (tab == 0) ? pong_binds.mouse_1p : breaker_binds.mouse_aim;
			draw_text_left(tab == 0 ? "MOUSE 1P CONTROL" : "MOUSE AIM CONTROL", -60.f, ry, 0.36f, sel ? text : text_dim);
			draw_text_centered(m_on ? "ON" : "OFF", 40.f, ry, 0.36f, m_on ? 0x22C55E : text_dim);
		} else if (i == num_binds + 1) {
			draw_text_centered("RESET DEFAULTS", 0.f, ry, 0.36f, sel ? 0xF59E0B : text_dim);
		} else {
			draw_text_centered("BACK", 0.f, ry, 0.36f, sel ? accent : text);
		}
	}

	draw_text_centered("ENTER/CLICK: REBIND   LEFT/RIGHT: SWITCH GAME   ESC: CANCEL/BACK",
	                   0.f, -40.5f, 0.30f, text_dim);
	return false;
}


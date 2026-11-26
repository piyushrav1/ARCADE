#include "settings_ui.h"
#include "keybinds.h"
#include "themes.h"
#include "save.h"
#include "renderer.h"

enum Settings_Tab {
	SET_AUDIO = 0,
	SET_GRAPHICS = 1,
	SET_THEME = 2,
	SET_CONTROLS = 3
};

global_variable int set_tab = SET_AUDIO;
global_variable int set_row = 0;
global_variable int ctrl_game_tab = 0;
global_variable bool ctrl_listening = false;

internal bool
in_rect(float px, float py, float cx, float cy, float hw, float hh) {
	return absf(px - cx) <= hw && absf(py - cy) <= hh;
}

internal void
draw_vol_bar(float cx, float cy, int pct, u32 fill_col, u32 empty_col) {
	int filled = (pct + 5) / 10;
	for (int s = 0; s < 10; s++) {
		float sx = cx - 15.75f + s * 3.5f;
		draw_rect(sx, cy, 1.3f, 1.3f, (s < filled) ? fill_col : empty_col);
	}
}

bool
update_and_draw_settings_ui(Input* input, int context,
                            u32 bg_outer, u32 panel_bg, u32 border, u32 card_bg,
                            u32 text, u32 text_dim, u32 accent) {
	if (context == 0) ctrl_game_tab = 0;
	else if (context == 1) ctrl_game_tab = 1;

	const int num_ctrl_binds = (ctrl_game_tab == 0) ? 5 : 4;
	int total_rows = 7;
	if (set_tab == SET_AUDIO)         total_rows = 7; // Master, Music, SFX, Mute, Hub BGM, Reset, Back
	else if (set_tab == SET_GRAPHICS) total_rows = 5; // Display, Size, Scanlines, Reset, Back
	else if (set_tab == SET_THEME)    total_rows = 3; // Theme, Reset, Back
	else                              total_rows = num_ctrl_binds + 3; // Binds + Mouse + Reset + Back

	if (set_row >= total_rows) set_row = total_rows - 1;

	if (ctrl_listening) {
		int k = input->last_key_pressed;
		if (k != 0) {
			if (k == VK_ESCAPE) {
				ctrl_listening = false;
				play_tone(320, 40, 0.14f);
			} else if (set_tab == SET_CONTROLS && set_row < num_ctrl_binds) {
				int* bind_ptrs[5] = {};
				if (ctrl_game_tab == 0) {
					bind_ptrs[0] = &pong_binds.p1_up;
					bind_ptrs[1] = &pong_binds.p1_down;
					bind_ptrs[2] = &pong_binds.p2_up;
					bind_ptrs[3] = &pong_binds.p2_down;
					bind_ptrs[4] = &pong_binds.pause;
				} else {
					bind_ptrs[0] = &breaker_binds.left;
					bind_ptrs[1] = &breaker_binds.right;
					bind_ptrs[2] = &breaker_binds.action;
					bind_ptrs[3] = &breaker_binds.pause;
				}
				*bind_ptrs[set_row] = k;
				ctrl_listening = false;
				play_tone(660, 55, 0.18f);
			}
		}
	} else {
		if (vk_pressed(input, VK_ESCAPE)) {
			ctrl_listening = false;
			set_row = 0;
			save_user_data();
			play_tone(400, 40, 0.14f);
			return true;
		}
		if (vk_pressed(input, 'Q') || vk_pressed(input, VK_TAB)) {
			int step = vk_pressed(input, 'Q') ? 3 : 1;
			set_tab = (set_tab + step) % 4;
			set_row = 0;
			play_tone(520, 35, 0.14f);
			return false;
		}
		if (vk_pressed(input, 'E')) {
			set_tab = (set_tab + 1) % 4;
			set_row = 0;
			play_tone(520, 35, 0.14f);
			return false;
		}
		if (vk_pressed(input, VK_UP) || vk_pressed(input, 'W')) {
			set_row = (set_row + total_rows - 1) % total_rows;
			play_tone(440, 30, 0.12f);
		}
		if (vk_pressed(input, VK_DOWN) || vk_pressed(input, 'S')) {
			set_row = (set_row + 1) % total_rows;
			play_tone(440, 30, 0.12f);
		}

		int dir = 0;
		if (vk_pressed(input, VK_LEFT)  || vk_pressed(input, 'A')) dir = -1;
		if (vk_pressed(input, VK_RIGHT) || vk_pressed(input, 'D')) dir = 1;

		float mx = input->mouse_x;
		float my = input->mouse_y;
		bool activate = vk_pressed(input, VK_RETURN) || vk_pressed(input, VK_SPACE);

		// Top tab clicks
		if (vk_pressed(input, VK_LBUTTON)) {
			for (int t = 0; t < 4; t++) {
				float tx = -51.f + t * 34.f;
				if (in_rect(mx, my, tx, 26.5f, 15.f, 2.8f)) {
					set_tab = t;
					set_row = 0;
					play_tone(520, 35, 0.14f);
					return false;
				}
			}
			if (set_tab == SET_CONTROLS && context < 0) {
				if (in_rect(mx, my, -22.f, 19.5f, 18.f, 2.4f)) { ctrl_game_tab = 0; set_row = 0; play_tone(500, 30, 0.14f); return false; }
				if (in_rect(mx, my,  22.f, 19.5f, 18.f, 2.4f)) { ctrl_game_tab = 1; set_row = 0; play_tone(500, 30, 0.14f); return false; }
			}
		}

		float list_top = (set_tab == SET_CONTROLS && context < 0) ? 12.5f : 16.5f;
		float list_step = (set_tab == SET_CONTROLS) ? 6.5f : 7.5f;

		for (int i = 0; i < total_rows; i++) {
			float ry = list_top - i * list_step;
			if (in_rect(mx, my, 0.f, ry, 66.f, 2.9f)) {
				if (input->mouse_moved && set_row != i) {
					set_row = i;
					play_tone(440, 24, 0.10f);
				}
				if (vk_pressed(input, VK_LBUTTON)) {
					set_row = i;
					if (set_tab == SET_AUDIO && i < 3 && in_rect(mx, my, 22.f, ry, 18.f, 2.5f)) {
						float norm = clampf(0.f, (mx - 4.5f) / 35.f, 1.f);
						int snapped = (mx < 5.5f) ? 0 : (((int)(norm * 9.f + 0.5f) + 1) * 10);
						snapped = clamp(0, snapped, 100);
						if (i == 0) set_master_volume(snapped);
						else if (i == 1) set_music_volume(snapped);
						else set_sfx_volume(snapped);
						play_tone(580, 40, 0.16f);
					} else {
						activate = true;
					}
				}
			}
		}

		// Handle row actions
		if (set_tab == SET_AUDIO) {
			if (dir != 0) {
				if (set_row == 0) set_master_volume(get_master_volume() + dir * 10);
				else if (set_row == 1) set_music_volume(get_music_volume() + dir * 10);
				else if (set_row == 2) set_sfx_volume(get_sfx_volume() + dir * 10);
				else if (set_row == 3) set_audio_muted(!is_audio_muted());
				else if (set_row == 4) { user_stats.hub_bgm_on = !user_stats.hub_bgm_on; if (context < 0) set_bgm_enabled(user_stats.hub_bgm_on); }
				play_tone(580, 40, 0.16f);
			}
			if (activate) {
				if (set_row == 0) set_master_volume((get_master_volume() + 10) % 110);
				else if (set_row == 1) set_music_volume((get_music_volume() + 10) % 110);
				else if (set_row == 2) set_sfx_volume((get_sfx_volume() + 10) % 110);
				else if (set_row == 3) set_audio_muted(!is_audio_muted());
				else if (set_row == 4) { user_stats.hub_bgm_on = !user_stats.hub_bgm_on; if (context < 0) set_bgm_enabled(user_stats.hub_bgm_on); }
				else if (set_row == 5) {
					set_master_volume(100); set_music_volume(80); set_sfx_volume(100);
					set_audio_muted(false); user_stats.hub_bgm_on = true;
					if (context < 0) set_bgm_enabled(true);
				} else {
					set_row = 0; save_user_data(); play_tone(400, 40, 0.14f); return true;
				}
				play_tone(580, 40, 0.16f);
			}
		} else if (set_tab == SET_GRAPHICS) {
			if (dir != 0 || activate) {
				if (set_row == 0) set_fullscreen(!is_fullscreen());
				else if (set_row == 1) set_windowed_res((get_windowed_res() + (dir < 0 ? 2 : 1)) % 3);
				else if (set_row == 2) set_crt_scanlines(!is_crt_scanlines());
				else if (set_row == 3 && activate) { set_fullscreen(true); set_windowed_res(0); set_crt_scanlines(false); }
				else if (set_row == 4 && activate) { set_row = 0; save_user_data(); play_tone(400, 40, 0.14f); return true; }
				play_tone(560, 40, 0.15f);
			}
		} else if (set_tab == SET_THEME) {
			if (dir != 0 || activate) {
				if (set_row == 0) {
					if (context < 0) {
						user_stats.hub_theme = (user_stats.hub_theme + (dir < 0 ? 2 : 1)) % 3;
					} else {
						int* ptr = (context == 0) ? &user_stats.pong_theme : &user_stats.breaker_theme;
						int cur = *ptr + 1; // 0..3
						cur = (cur + (dir < 0 ? 3 : 1)) % 4;
						*ptr = cur - 1;
					}
					play_tone(520, 40, 0.15f);
				} else if (set_row == 1 && activate) {
					if (context < 0) user_stats.hub_theme = 0;
					else if (context == 0) user_stats.pong_theme = -1;
					else user_stats.breaker_theme = -1;
					play_tone(600, 50, 0.16f);
				} else if (set_row == 2 && activate) {
					set_row = 0; save_user_data(); play_tone(400, 40, 0.14f); return true;
				}
			}
		} else {
			if (context < 0 && dir != 0) {
				ctrl_game_tab = 1 - ctrl_game_tab;
				set_row = 0;
				play_tone(520, 35, 0.14f);
				return false;
			}
			if (activate) {
				if (set_row < num_ctrl_binds) {
					ctrl_listening = true;
					play_tone(580, 40, 0.15f);
				} else if (set_row == num_ctrl_binds) {
					if (ctrl_game_tab == 0) pong_binds.mouse_1p = !pong_binds.mouse_1p;
					else                    breaker_binds.mouse_aim = !breaker_binds.mouse_aim;
					play_tone(520, 40, 0.15f);
				} else if (set_row == num_ctrl_binds + 1) {
					if (ctrl_game_tab == 0) reset_pong_binds();
					else                    reset_breaker_binds();
					play_tone(620, 60, 0.18f);
				} else {
					set_row = 0; save_user_data(); play_tone(400, 40, 0.14f); return true;
				}
			}
		}
	}

	// Render Settings UI
	clear_screen(bg_outer);
	draw_rect(0.f, 0.f, 86.2f, 46.2f, border);
	draw_rect(0.f, 0.f, 85.0f, 45.0f, panel_bg);

	draw_text_centered("SETTINGS", 0.f, 36.f, 0.85f, accent);

	const char* TAB_NAMES[4] = { "AUDIO", "GRAPHICS", "THEME", "CONTROLS" };
	for (int t = 0; t < 4; t++) {
		float tx = -51.f + t * 34.f;
		bool active = (t == set_tab);
		draw_rect(tx, 26.5f, 15.4f, 2.8f, active ? accent : border);
		draw_rect(tx, 26.5f, 15.0f, 2.4f, card_bg);
		draw_text_centered(TAB_NAMES[t], tx, 26.5f, 0.33f, active ? text : text_dim);
	}

	float list_top = (set_tab == SET_CONTROLS && context < 0) ? 12.5f : 16.5f;
	float list_step = (set_tab == SET_CONTROLS) ? 6.5f : 7.5f;

	if (set_tab == SET_CONTROLS && context < 0) {
		draw_rect(-22.f, 19.5f, 18.4f, 2.4f, ctrl_game_tab == 0 ? accent : border);
		draw_rect(-22.f, 19.5f, 18.0f, 2.0f, card_bg);
		draw_text_centered("PONG ARENA", -22.f, 19.5f, 0.30f, ctrl_game_tab == 0 ? text : text_dim);

		draw_rect(22.f, 19.5f, 18.4f, 2.4f, ctrl_game_tab == 1 ? accent : border);
		draw_rect(22.f, 19.5f, 18.0f, 2.0f, card_bg);
		draw_text_centered("NEON BREAKER", 22.f, 19.5f, 0.30f, ctrl_game_tab == 1 ? text : text_dim);
	}

	for (int i = 0; i < total_rows; i++) {
		float ry = list_top - i * list_step;
		bool sel = (i == set_row);
		bool listening = (set_tab == SET_CONTROLS && sel && ctrl_listening);

		draw_rect(0.f, ry, 66.4f, 2.9f, listening ? 0xFACC15 : (sel ? accent : border));
		draw_rect(0.f, ry, 66.0f, 2.5f, card_bg);

		if (set_tab == SET_AUDIO) {
			const char* alabels[3] = { "MASTER VOLUME", "MUSIC VOLUME", "SFX VOLUME" };
			int avals[3] = { get_master_volume(), get_music_volume(), get_sfx_volume() };
			if (i < 3) {
				draw_text_left(alabels[i], -60.f, ry, 0.34f, sel ? text : text_dim);
				draw_vol_bar(22.f, ry, avals[i], sel ? accent : 0x22C55E, border);
				draw_number(avals[i], 54.f, ry, 0.34f, sel ? text : text_dim);
			} else if (i == 3) {
				bool m = is_audio_muted();
				draw_text_left("MUTE ALL AUDIO", -60.f, ry, 0.34f, sel ? text : text_dim);
				draw_text_centered(m ? "ON" : "OFF", 40.f, ry, 0.34f, m ? 0xEF4444 : 0x22C55E);
			} else if (i == 4) {
				draw_text_left("HUB MENU MUSIC", -60.f, ry, 0.34f, sel ? text : text_dim);
				draw_text_centered(user_stats.hub_bgm_on ? "ON" : "OFF", 40.f, ry, 0.34f, user_stats.hub_bgm_on ? 0x22C55E : text_dim);
			} else if (i == 5) {
				draw_text_centered("RESET DEFAULTS", 0.f, ry, 0.34f, sel ? 0xF59E0B : text_dim);
			} else {
				draw_text_centered("BACK", 0.f, ry, 0.34f, sel ? accent : text);
			}
		} else if (set_tab == SET_GRAPHICS) {
			const char* RES_LABELS[3] = { "1280X720", "1600X900", "1920X1080" };
			bool fs = is_fullscreen();
			bool scan = is_crt_scanlines();
			if (i == 0) {
				draw_text_left("DISPLAY MODE", -60.f, ry, 0.34f, sel ? text : text_dim);
				draw_text_centered(fs ? "FULLSCREEN" : "WINDOWED", 40.f, ry, 0.34f, accent);
			} else if (i == 1) {
				draw_text_left("WINDOWED SIZE", -60.f, ry, 0.34f, sel ? text : text_dim);
				draw_text_centered(RES_LABELS[get_windowed_res()], 40.f, ry, 0.34f, fs ? text_dim : accent);
			} else if (i == 2) {
				draw_text_left("CRT SCANLINES", -60.f, ry, 0.34f, sel ? text : text_dim);
				draw_text_centered(scan ? "ON" : "OFF", 40.f, ry, 0.34f, scan ? 0x22C55E : text_dim);
			} else if (i == 3) {
				draw_text_centered("RESET DEFAULTS", 0.f, ry, 0.34f, sel ? 0xF59E0B : text_dim);
			} else {
				draw_text_centered("BACK", 0.f, ry, 0.34f, sel ? accent : text);
			}
		} else if (set_tab == SET_THEME) {
			if (i == 0) {
				int val = (context < 0) ? user_stats.hub_theme :
				          (context == 0 ? user_stats.pong_theme : user_stats.breaker_theme);
				const char* tname = (val < 0) ? "FOLLOW HUB" : THEMES[val].name;
				draw_text_left(context < 0 ? "GLOBAL THEME" : "GAME THEME", -60.f, ry, 0.34f, sel ? text : text_dim);
				draw_text_centered(tname, 40.f, ry, 0.34f, accent);
			} else if (i == 1) {
				draw_text_centered("RESET DEFAULTS", 0.f, ry, 0.34f, sel ? 0xF59E0B : text_dim);
			} else {
				draw_text_centered("BACK", 0.f, ry, 0.34f, sel ? accent : text);
			}
		} else {
			const char* clabels[5] = {};
			int cvals[5] = {};
			if (ctrl_game_tab == 0) {
				clabels[0] = "P1 MOVE UP";   cvals[0] = pong_binds.p1_up;
				clabels[1] = "P1 MOVE DOWN"; cvals[1] = pong_binds.p1_down;
				clabels[2] = "P2 MOVE UP";   cvals[2] = pong_binds.p2_up;
				clabels[3] = "P2 MOVE DOWN"; cvals[3] = pong_binds.p2_down;
				clabels[4] = "PAUSE";        cvals[4] = pong_binds.pause;
			} else {
				clabels[0] = "MOVE LEFT";      cvals[0] = breaker_binds.left;
				clabels[1] = "MOVE RIGHT";     cvals[1] = breaker_binds.right;
				clabels[2] = "LAUNCH / LASER"; cvals[2] = breaker_binds.action;
				clabels[3] = "PAUSE";          cvals[3] = breaker_binds.pause;
			}
			if (i < num_ctrl_binds) {
				draw_text_left(clabels[i], -60.f, ry, 0.33f, sel ? text : text_dim);
				draw_text_centered(listening ? "PRESS ANY KEY..." : vk_name(cvals[i]),
				                   40.f, ry, 0.33f, listening ? 0xFACC15 : accent);
			} else if (i == num_ctrl_binds) {
				bool m_on = (ctrl_game_tab == 0) ? pong_binds.mouse_1p : breaker_binds.mouse_aim;
				draw_text_left(ctrl_game_tab == 0 ? "MOUSE 1P CONTROL" : "MOUSE AIM CONTROL", -60.f, ry, 0.33f, sel ? text : text_dim);
				draw_text_centered(m_on ? "ON" : "OFF", 40.f, ry, 0.33f, m_on ? 0x22C55E : text_dim);
			} else if (i == num_ctrl_binds + 1) {
				draw_text_centered("RESET DEFAULTS", 0.f, ry, 0.33f, sel ? 0xF59E0B : text_dim);
			} else {
				draw_text_centered("BACK", 0.f, ry, 0.33f, sel ? accent : text);
			}
		}
	}

	draw_text_centered("Q/E OR CLICK TAB: SWITCH   LEFT/RIGHT: ADJUST   ESC: SAVE AND BACK",
	                   0.f, -40.5f, 0.28f, text_dim);
	return false;
}

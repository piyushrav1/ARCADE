#include "audio_ui.h"
#include "keybinds.h"
#include "renderer.h"

global_variable int audio_row = 0;

internal bool
in_rect(float px, float py, float cx, float cy, float hw, float hh) {
	return absf(px - cx) <= hw && absf(py - cy) <= hh;
}

internal void
draw_vol_bar(float cx, float cy, int pct, u32 fill_col, u32 empty_col) {
	// 10 segments from cx - 16 to cx + 16
	int filled = (pct + 5) / 10;
	for (int s = 0; s < 10; s++) {
		float sx = cx - 15.75f + s * 3.5f;
		draw_rect(sx, cy, 1.3f, 1.3f, (s < filled) ? fill_col : empty_col);
	}
}

bool
update_and_draw_audio_ui(Input* input,
                         u32 bg_outer, u32 panel_bg, u32 border, u32 card_bg,
                         u32 text, u32 text_dim, u32 accent) {
	const int total_rows = 6; // Master, Music, SFX, Mute All, Reset, Back

	if (vk_pressed(input, VK_ESCAPE)) {
		audio_row = 0;
		play_tone(400, 40, 0.14f);
		return true;
	}
	if (vk_pressed(input, VK_UP) || vk_pressed(input, 'W')) {
		audio_row = (audio_row + total_rows - 1) % total_rows;
		play_tone(440, 30, 0.12f);
	}
	if (vk_pressed(input, VK_DOWN) || vk_pressed(input, 'S')) {
		audio_row = (audio_row + 1) % total_rows;
		play_tone(440, 30, 0.12f);
	}

	int delta = 0;
	if (vk_pressed(input, VK_LEFT)  || vk_pressed(input, 'A')) delta = -10;
	if (vk_pressed(input, VK_RIGHT) || vk_pressed(input, 'D')) delta = 10;

	float mx = input->mouse_x;
	float my = input->mouse_y;
	bool activate = vk_pressed(input, VK_RETURN) || vk_pressed(input, VK_SPACE);

	for (int i = 0; i < total_rows; i++) {
		float ry = 20.f - i * 8.2f;
		if (in_rect(mx, my, 0.f, ry, 66.f, 3.3f)) {
			if (input->mouse_moved && audio_row != i) {
				audio_row = i;
				play_tone(440, 24, 0.10f);
			}
			if (vk_pressed(input, VK_LBUTTON)) {
				audio_row = i;
				if (i < 3 && in_rect(mx, my, 22.f, ry, 18.f, 2.5f)) {
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

	if (delta != 0) {
		if (audio_row == 0) set_master_volume(get_master_volume() + delta);
		else if (audio_row == 1) set_music_volume(get_music_volume() + delta);
		else if (audio_row == 2) set_sfx_volume(get_sfx_volume() + delta);
		else if (audio_row == 3) set_audio_muted(!is_audio_muted());
		play_tone(580, 40, 0.16f);
	}

	if (activate) {
		if (audio_row < 3) {
			delta = 10;
			if (audio_row == 0) set_master_volume((get_master_volume() + 10) % 110);
			else if (audio_row == 1) set_music_volume((get_music_volume() + 10) % 110);
			else set_sfx_volume((get_sfx_volume() + 10) % 110);
			play_tone(580, 40, 0.16f);
		} else if (audio_row == 3) {
			set_audio_muted(!is_audio_muted());
			play_tone(520, 40, 0.15f);
		} else if (audio_row == 4) {
			set_master_volume(100);
			set_music_volume(80);
			set_sfx_volume(100);
			set_audio_muted(false);
			play_tone(620, 60, 0.18f);
		} else {
			audio_row = 0;
			play_tone(400, 40, 0.14f);
			return true;
		}
	}

	clear_screen(bg_outer);
	draw_rect(0.f, 0.f, 86.2f, 46.2f, border);
	draw_rect(0.f, 0.f, 85.0f, 45.0f, panel_bg);

	draw_text_centered("SOUND SETTINGS", 0.f, 35.f, 0.85f, accent);

	const char* labels[3] = { "MASTER VOLUME", "MUSIC VOLUME", "SFX VOLUME" };
	int vals[3] = { get_master_volume(), get_music_volume(), get_sfx_volume() };

	for (int i = 0; i < total_rows; i++) {
		float ry = 20.f - i * 8.2f;
		bool sel = (i == audio_row);

		draw_rect(0.f, ry, 66.4f, 3.3f, sel ? accent : border);
		draw_rect(0.f, ry, 66.0f, 2.9f, card_bg);

		if (i < 3) {
			draw_text_left(labels[i], -60.f, ry, 0.36f, sel ? text : text_dim);
			draw_vol_bar(22.f, ry, vals[i], sel ? accent : 0x22C55E, border);
			draw_number(vals[i], 54.f, ry, 0.36f, sel ? text : text_dim);
		} else if (i == 3) {
			bool m = is_audio_muted();
			draw_text_left("MUTE ALL AUDIO", -60.f, ry, 0.36f, sel ? text : text_dim);
			draw_text_centered(m ? "ON" : "OFF", 40.f, ry, 0.36f, m ? 0xEF4444 : 0x22C55E);
		} else if (i == 4) {
			draw_text_centered("RESET DEFAULTS", 0.f, ry, 0.36f, sel ? 0xF59E0B : text_dim);
		} else {
			draw_text_centered("BACK", 0.f, ry, 0.36f, sel ? accent : text);
		}
	}

	draw_text_centered("LEFT/RIGHT OR CLICK BAR: ADJUST   ENTER: TOGGLE   ESC: BACK",
	                   0.f, -40.5f, 0.30f, text_dim);
	return false;
}


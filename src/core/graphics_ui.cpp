#include "graphics_ui.h"
#include "keybinds.h"
#include "renderer.h"

global_variable int gfx_row = 0;

internal bool
in_rect(float px, float py, float cx, float cy, float hw, float hh) {
	return absf(px - cx) <= hw && absf(py - cy) <= hh;
}

bool
update_and_draw_graphics_ui(Input* input,
                            u32 bg_outer, u32 panel_bg, u32 border, u32 card_bg,
                            u32 text, u32 text_dim, u32 accent) {
	const int total_rows = 5; // Display Mode, Windowed Size, CRT Scanlines, Reset, Back

	if (vk_pressed(input, VK_ESCAPE)) {
		gfx_row = 0;
		play_tone(400, 40, 0.14f);
		return true;
	}
	if (vk_pressed(input, VK_UP) || vk_pressed(input, 'W')) {
		gfx_row = (gfx_row + total_rows - 1) % total_rows;
		play_tone(440, 30, 0.12f);
	}
	if (vk_pressed(input, VK_DOWN) || vk_pressed(input, 'S')) {
		gfx_row = (gfx_row + 1) % total_rows;
		play_tone(440, 30, 0.12f);
	}

	int dir = 0;
	if (vk_pressed(input, VK_LEFT)  || vk_pressed(input, 'A')) dir = -1;
	if (vk_pressed(input, VK_RIGHT) || vk_pressed(input, 'D')) dir = 1;

	float mx = input->mouse_x;
	float my = input->mouse_y;
	bool activate = vk_pressed(input, VK_RETURN) || vk_pressed(input, VK_SPACE);

	for (int i = 0; i < total_rows; i++) {
		float ry = 18.f - i * 8.5f;
		if (in_rect(mx, my, 0.f, ry, 66.f, 3.3f)) {
			if (input->mouse_moved && gfx_row != i) {
				gfx_row = i;
				play_tone(440, 24, 0.10f);
			}
			if (vk_pressed(input, VK_LBUTTON)) {
				gfx_row = i;
				activate = true;
			}
		}
	}

	if (dir != 0 || activate) {
		if (gfx_row == 0) {
			set_fullscreen(!is_fullscreen());
			play_tone(560, 45, 0.16f);
		} else if (gfx_row == 1) {
			int step = (dir < 0) ? 2 : 1;
			set_windowed_res((get_windowed_res() + step) % 3);
			play_tone(560, 45, 0.16f);
		} else if (gfx_row == 2) {
			set_crt_scanlines(!is_crt_scanlines());
			play_tone(520, 40, 0.15f);
		} else if (gfx_row == 3 && activate) {
			set_fullscreen(true);
			set_windowed_res(0);
			set_crt_scanlines(false);
			play_tone(620, 60, 0.18f);
		} else if (gfx_row == 4 && activate) {
			gfx_row = 0;
			play_tone(400, 40, 0.14f);
			return true;
		}
	}

	clear_screen(bg_outer);
	draw_rect(0.f, 0.f, 86.2f, 46.2f, border);
	draw_rect(0.f, 0.f, 85.0f, 45.0f, panel_bg);

	draw_text_centered("GRAPHICS SETTINGS", 0.f, 35.f, 0.85f, accent);

	const char* RES_LABELS[3] = { "1280X720", "1600X900", "1920X1080" };
	bool fs = is_fullscreen();
	bool scan = is_crt_scanlines();

	for (int i = 0; i < total_rows; i++) {
		float ry = 18.f - i * 8.5f;
		bool sel = (i == gfx_row);

		draw_rect(0.f, ry, 66.4f, 3.3f, sel ? accent : border);
		draw_rect(0.f, ry, 66.0f, 2.9f, card_bg);

		if (i == 0) {
			draw_text_left("DISPLAY MODE", -60.f, ry, 0.36f, sel ? text : text_dim);
			draw_text_centered(fs ? "FULLSCREEN" : "WINDOWED", 40.f, ry, 0.36f, accent);
		} else if (i == 1) {
			draw_text_left("WINDOWED SIZE", -60.f, ry, 0.36f, sel ? text : text_dim);
			draw_text_centered(RES_LABELS[get_windowed_res()], 40.f, ry, 0.36f, fs ? text_dim : accent);
		} else if (i == 2) {
			draw_text_left("CRT SCANLINES", -60.f, ry, 0.36f, sel ? text : text_dim);
			draw_text_centered(scan ? "ON" : "OFF", 40.f, ry, 0.36f, scan ? 0x22C55E : text_dim);
		} else if (i == 3) {
			draw_text_centered("RESET DEFAULTS", 0.f, ry, 0.36f, sel ? 0xF59E0B : text_dim);
		} else {
			draw_text_centered("BACK", 0.f, ry, 0.36f, sel ? accent : text);
		}
	}

	draw_text_centered("LEFT/RIGHT OR CLICK: CHANGE   F11: FULLSCREEN   ESC: BACK",
	                   0.f, -40.5f, 0.30f, text_dim);
	return false;
}

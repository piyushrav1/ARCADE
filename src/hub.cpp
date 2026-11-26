#include <math.h>
#include "utils.h"
#include "renderer.h"
#include "keybinds.h"
#include "audio_ui.h"
#include "game_interface.h"

#define pressed(b) (input->buttons[b].is_down && input->buttons[b].changed)

enum Hub_State {
	HUB_SPLASH,
	HUB_MENU,
	HUB_CONTROLS,
	HUB_SOUND
};

enum Active_Scene {
	SCENE_HUB,
	SCENE_PONG,
	SCENE_BREAKER
};

struct Game_Info {
	const char* title;
	const char* subtitle;
	const char* players;
	u32 accent;
	int id; // 0 = Pong, 1 = Breaker
};

global_variable const Game_Info GAMES[] = {
	{ "PONG ARENA",   "CLASSIC PADDLE DUEL WITH POWERUPS",   "1P / 2P", 0x38BDF8, 0 },
	{ "NEON BREAKER", "8 LEVELS - TNT - LASERS - MULTIBALL", "1P",      0x22D3EE, 1 },
};
global_variable const int NUM_GAMES = (int)(sizeof(GAMES) / sizeof(GAMES[0]));

struct Theme {
	const char* name;
	u32 bg_outer;
	u32 panel_bg;
	u32 panel_border;
	u32 card_bg;
	u32 card_selected;
	u32 text;
	u32 text_dim;
	u32 accent;
};

global_variable const Theme THEMES[3] = {
	{ "CYBER",     0x0B1120, 0x0F172A, 0x334155, 0x162033, 0x1E293B, 0xF1F5F9, 0x64748B, 0x22D3EE },
	{ "CRT",       0x020802, 0x051405, 0x166534, 0x082008, 0x14532D, 0xBBF7D0, 0x22C55E, 0x4ADE80 },
	{ "SYNTHWAVE", 0x120624, 0x1E0B36, 0x7E22CE, 0x2A104B, 0x4C1D95, 0xFAE8FF, 0xA855F7, 0xF472B6 },
};

struct Star {
	float x, y, speed, size;
};

#define HUB_MAX_STARS 36
global_variable Star stars[HUB_MAX_STARS];

global_variable Hub_State hub_state = HUB_SPLASH;
global_variable Active_Scene active_scene = SCENE_HUB;
global_variable float splash_timer = 0.f;
global_variable int   splash_stage = 0;

global_variable int  selected_idx = 0;
global_variable int  controls_tab = 0;
global_variable int  theme_idx = 0;
global_variable bool bgm_on = true;
global_variable float anim_time = 0.f;

internal void
sfx(int freq_hz, int duration_ms, float volume = 0.22f, float freq_end_hz = 0.f) {
	if (!is_audio_muted()) play_tone(freq_hz, duration_ms, volume, freq_end_hz);
}

internal bool
point_in_rect(float px, float py, float cx, float cy, float hw, float hh) {
	return absf(px - cx) <= hw && absf(py - cy) <= hh;
}

internal void
draw_game_icon(int id, float cx, float cy, u32 accent) {
	draw_rect(cx, cy, 5.8f, 5.8f, 0x050814);
	if (id == 0) {
		float bob = sinf(anim_time * 3.5f) * 1.6f;
		draw_rect(cx, cy, 0.22f, 4.2f, 0x334155);
		draw_rect(cx - 3.8f, cy + bob * 0.6f, 0.65f, 2.0f, accent);
		draw_rect(cx + 3.8f, cy - bob * 0.6f, 0.65f, 2.0f, 0xF59E0B);
		draw_rect(cx + sinf(anim_time * 2.2f) * 2.0f, cy + bob, 0.75f, 0.75f, 0xF8FAFC);
	} else {
		draw_rect(cx - 2.4f, cy + 2.6f, 1.8f, 0.8f, accent);
		draw_rect(cx + 2.4f, cy + 2.6f, 1.8f, 0.8f, 0xA855F7);
		draw_rect(cx,        cy + 0.6f, 1.8f, 0.8f, 0xEF4444);
		draw_rect(cx, cy - 3.6f, 2.8f, 0.6f, accent);
		draw_rect(cx + sinf(anim_time * 2.5f) * 1.5f, cy - 1.5f, 0.7f, 0.7f, 0xF8FAFC);
	}
}

internal void
draw_pill(float cx, float cy, float hw, float hh, const char* label, u32 bg, u32 border, u32 fg, bool hovered) {
	draw_rect(cx, cy, hw + 0.3f, hh + 0.3f, hovered ? 0xF8FAFC : border);
	draw_rect(cx, cy, hw, hh, bg);
	draw_text_centered(label, cx, cy, 0.30f, fg);
}

internal void
launch_selected_game() {
	sfx(440, 180, 0.24f, 1080.f);
	set_bgm_enabled(false);
	render_offset_x = 0.f;
	render_offset_y = 0.f;
	if (GAMES[selected_idx].id == 0) {
		Pong::init();
		active_scene = SCENE_PONG;
	} else {
		Breaker::init();
		active_scene = SCENE_BREAKER;
	}
}

void
hub_init() {
	u32 rng = 4242u;
	for (int i = 0; i < HUB_MAX_STARS; i++) {
		rng = rng * 1664525u + 1013904223u;
		stars[i].x = (float)((s32)(rng % 170) - 85);
		rng = rng * 1664525u + 1013904223u;
		stars[i].y = (float)((s32)(rng % 94) - 47);
		stars[i].speed = 2.5f + (float)(i % 5) * 1.8f;
		stars[i].size = (i % 3 == 0) ? 0.38f : 0.24f;
	}
	hub_state = HUB_SPLASH;
	active_scene = SCENE_HUB;
	splash_timer = 0.f;
	splash_stage = 0;
	set_bgm_enabled(false);
}

void
hub_update(Input* input, float dt, float fps) {
	if (active_scene == SCENE_PONG) {
		if (Pong::update(input, dt, fps) == GAME_EXIT_TO_HUB) {
			active_scene = SCENE_HUB;
			set_bgm_enabled(bgm_on);
			sfx(520, 60, 0.18f);
		}
		return;
	}
	if (active_scene == SCENE_BREAKER) {
		if (Breaker::update(input, dt) == GAME_EXIT_TO_HUB) {
			active_scene = SCENE_HUB;
			set_bgm_enabled(bgm_on);
			sfx(520, 60, 0.18f);
		}
		return;
	}

	anim_time += dt;
	for (int i = 0; i < HUB_MAX_STARS; i++) {
		stars[i].y -= stars[i].speed * dt;
		if (stars[i].y < -48.f) stars[i].y = 48.f;
	}

	const Theme& th = THEMES[theme_idx];

	if (hub_state == HUB_SPLASH) {
		splash_timer += dt;

		if (splash_stage == 0) { splash_stage = 1; sfx(110, 260, 0.22f, 880.f); }
		else if (splash_stage == 1 && splash_timer >= 0.55f) { splash_stage = 2; sfx(740, 35, 0.14f); }
		else if (splash_stage == 2 && splash_timer >= 1.05f) { splash_stage = 3; sfx(740, 35, 0.14f); }
		else if (splash_stage == 3 && splash_timer >= 1.55f) { splash_stage = 4; sfx(880, 45, 0.16f); }
		else if (splash_stage == 4 && splash_timer >= 2.05f) { splash_stage = 5; sfx(523, 120, 0.22f, 1046.f); }

		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE) ||
		    pressed(BUTTON_ESC) || pressed(BUTTON_MOUSE_LEFT) || splash_timer >= 3.2f) {
			hub_state = HUB_MENU;
			set_bgm_enabled(bgm_on);
			return;
		}

		clear_screen(0x020408);
		if (splash_timer < 0.45f) {
			float t = splash_timer / 0.45f;
			float hw = clampf(4.f, t * 170.f, 85.f);
			float hh = (t < 0.5f) ? 0.5f : (t - 0.5f) * 2.f * 45.f;
			draw_rect(0.f, 0.f, hw, hh, th.panel_bg);
			draw_rect(0.f, 0.f, hw, 0.4f, 0xE0F2FE);
		} else {
			draw_rect(0.f, 0.f, 86.2f, 46.2f, th.panel_border);
			draw_rect(0.f, 0.f, 85.0f, 45.0f, th.panel_bg);
			if (splash_timer < 2.05f) {
				draw_text_left("ARCADE OS V1.0", -64.f, 22.f, 0.50f, th.accent);
				if (splash_timer >= 0.55f) draw_text_left("MEMORY CHECK ..... OK", -64.f, 12.f, 0.40f, th.text);
				if (splash_timer >= 1.05f) draw_text_left("AUDIO SYNTH ...... READY", -64.f, 4.f, 0.40f, th.text);
				if (splash_timer >= 1.55f) {
					draw_text_left("GAMES LOADED ..... ", -64.f, -4.f, 0.40f, 0x22C55E);
					draw_number(NUM_GAMES, 10.f, -4.f, 0.40f, th.accent);
				}
			} else {
				for (int i = 0; i < HUB_MAX_STARS; i++) {
					draw_rect_alpha(stars[i].x, stars[i].y, stars[i].size, stars[i].size, th.accent, 0.40f);
				}
				draw_text_centered("ARCADE HUB", 0.f, 8.f, 1.40f, th.accent);
				draw_text_centered("SYSTEM READY", 0.f, -10.f, 0.45f, th.text);
			}
			draw_text_centered("PRESS SPACE TO SKIP", 0.f, -39.f, 0.32f, th.text_dim);
		}
		return;
	}

	// HUB_CONTROLS
	if (hub_state == HUB_CONTROLS) {
		if (update_and_draw_keybinds_ui(input, &controls_tab, true,
		                                th.bg_outer, th.panel_bg, th.panel_border,
		                                th.card_bg, th.text, th.text_dim, th.accent)) {
			hub_state = HUB_MENU;
		}
		return;
	}

	// HUB_SOUND
	if (hub_state == HUB_SOUND) {
		if (update_and_draw_audio_ui(input,
		                             th.bg_outer, th.panel_bg, th.panel_border,
		                             th.card_bg, th.text, th.text_dim, th.accent)) {
			hub_state = HUB_MENU;
		}
		return;
	}

	// HUB_MENU
	if (pressed(BUTTON_ESC)) { request_quit(); return; }
	if (vk_pressed(input, VK_TAB)) {
		controls_tab = selected_idx;
		hub_state = HUB_CONTROLS;
		sfx(520, 40, 0.16f);
		return;
	}
	if (pressed(BUTTON_M)) {
		set_audio_muted(!is_audio_muted());
		if (!is_audio_muted()) sfx(580, 45, 0.18f);
	}
	if (pressed(BUTTON_B)) {
		bgm_on = !bgm_on;
		set_bgm_enabled(bgm_on);
		sfx(520, 40, 0.16f);
	}
	if (pressed(BUTTON_T) || pressed(BUTTON_RIGHT)) { theme_idx = (theme_idx + 1) % 3; sfx(520, 35, 0.15f); }
	if (pressed(BUTTON_LEFT))                         { theme_idx = (theme_idx + 2) % 3; sfx(520, 35, 0.15f); }
	if (pressed(BUTTON_UP) || pressed(BUTTON_W))      { selected_idx = (selected_idx + NUM_GAMES - 1) % NUM_GAMES; sfx(440, 35, 0.15f); }
	if (pressed(BUTTON_DOWN) || pressed(BUTTON_S))    { selected_idx = (selected_idx + 1) % NUM_GAMES; sfx(440, 35, 0.15f); }

	float mx = input->mouse_x;
	float my = input->mouse_y;
	bool hov_keys  = point_in_rect(mx, my, -5.f, 36.f, 10.f, 2.6f);
	bool hov_sound = point_in_rect(mx, my, 18.f, 36.f, 10.f, 2.6f);
	bool hov_theme = point_in_rect(mx, my, 41.f, 36.f, 10.f, 2.6f);
	bool hov_music = point_in_rect(mx, my, 64.f, 36.f, 10.f, 2.6f);

	if (pressed(BUTTON_MOUSE_LEFT)) {
		if (hov_keys) { controls_tab = selected_idx; hub_state = HUB_CONTROLS; sfx(520, 40, 0.16f); return; }
		else if (hov_sound) { hub_state = HUB_SOUND; sfx(520, 40, 0.16f); return; }
		else if (hov_theme) { theme_idx = (theme_idx + 1) % 3; sfx(520, 35, 0.15f); }
		else if (hov_music) { bgm_on = !bgm_on; set_bgm_enabled(bgm_on); sfx(520, 40, 0.16f); }
	}

	for (int i = 0; i < NUM_GAMES; i++) {
		float cy = 14.f - i * 16.5f;
		if (point_in_rect(mx, my, 0.f, cy, 74.f, 6.4f)) {
			if (input->mouse_moved && selected_idx != i) {
				selected_idx = i;
				sfx(440, 28, 0.12f);
			}
			if (pressed(BUTTON_MOUSE_LEFT)) {
				selected_idx = i;
				launch_selected_game();
				return;
			}
		}
	}

	if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE)) {
		launch_selected_game();
		return;
	}

	clear_screen(th.bg_outer);
	draw_rect(0.f, 0.f, 86.2f, 46.2f, th.panel_border);
	draw_rect(0.f, 0.f, 85.0f, 45.0f, th.panel_bg);

	for (int i = 0; i < HUB_MAX_STARS; i++) {
		draw_rect_alpha(stars[i].x, stars[i].y, stars[i].size, stars[i].size, th.accent, 0.30f);
	}

	draw_text_left("ARCADE HUB", -74.f, 36.f, 0.85f, th.accent);
	draw_rect(0.f, 29.5f, 74.f, 0.22f, th.panel_border);

	draw_pill(-5.f, 36.f, 10.f, 2.6f, "KEYS",  th.card_bg, th.panel_border, th.text, hov_keys);
	draw_pill(18.f, 36.f, 10.f, 2.6f, "SOUND", th.card_bg, th.panel_border, th.text, hov_sound);
	draw_pill(41.f, 36.f, 10.f, 2.6f, th.name, th.card_bg, th.panel_border, th.text, hov_theme);
	bool muted = is_audio_muted();
	const char* music_lbl = muted ? "MUTED" : (bgm_on ? "MUSIC:ON" : "MUSIC:OFF");
	u32 music_col = muted ? 0xEF4444 : (bgm_on ? 0x22C55E : th.text_dim);
	draw_pill(64.f, 36.f, 10.f, 2.6f, music_lbl, th.card_bg, th.panel_border, music_col, hov_music);

	for (int i = 0; i < NUM_GAMES; i++) {
		float cy = 14.f - i * 16.5f;
		const Game_Info& g = GAMES[i];
		bool is_sel = (i == selected_idx);

		draw_rect(0.f, cy, 74.6f, 6.4f, is_sel ? th.accent : th.panel_border);
		draw_rect(0.f, cy, 74.0f, 5.8f, is_sel ? th.card_selected : th.card_bg);

		if (is_sel) {
			float pulse = 0.5f * sinf(anim_time * 6.f);
			draw_text_centered(">", -78.f + pulse, cy, 0.58f, th.accent);
		}

		draw_game_icon(g.id, -64.f, cy, g.accent);
		draw_text_left(g.title, -53.f, cy + 2.1f, 0.52f, is_sel ? th.text : th.text_dim);
		draw_text_left(g.subtitle, -53.f, cy - 2.2f, 0.30f, th.text_dim);

		draw_text_centered(g.players, 42.f, cy + 1.9f, 0.34f, th.text_dim);
		draw_text_centered(is_sel ? "PLAY" : "READY", 62.f, cy - 1.9f, 0.36f, is_sel ? th.accent : 0x22C55E);
	}

	draw_text_centered("ENTER/CLICK: PLAY   TAB: KEYBINDS   B: MUSIC   T: THEME   M: MUTE   ESC: QUIT",
	                   0.f, -40.5f, 0.28f, th.text_dim);
}


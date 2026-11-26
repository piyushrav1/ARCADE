#include <math.h>
#include "utils.h"
#include "renderer.h"
#include "catalog.h"
#include "launcher.h"

#define pressed(b) (input->buttons[b].is_down && input->buttons[b].changed)

enum Launcher_State {
	LAUNCHER_SPLASH,
	LAUNCHER_MENU,
	LAUNCHER_LAUNCHING
};

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
	float x, y;
	float speed;
	float size;
};

struct Particle {
	float x, y;
	float vx, vy;
	float life;
	float max_life;
	u32 color;
};

#define MAX_STARS     36
#define MAX_PARTICLES 48

global_variable Star stars[MAX_STARS];
global_variable Particle particles[MAX_PARTICLES];
global_variable Game_Catalog catalog;

global_variable Launcher_State launcher_state = LAUNCHER_SPLASH;
global_variable float splash_timer = 0.f;
global_variable int   splash_stage_beeped = 0;
global_variable float launch_timer = 0.f;

global_variable int  selected_idx = 0;
global_variable int  scroll_offset = 0;
global_variable int  theme_idx = 0;
global_variable bool sound_muted = false;
global_variable bool bgm_on = true;

global_variable float anim_time = 0.f;
global_variable float toast_timer = 0.f;
global_variable char  toast_msg[64] = "";

internal void
sfx(int freq_hz, int duration_ms, float volume = 0.22f, float freq_end_hz = 0.f) {
	if (!sound_muted) play_tone(freq_hz, duration_ms, volume, freq_end_hz);
}

internal void
show_toast(const char* msg, float duration = 2.4f) {
	int i = 0;
	while (msg[i] && i < (int)sizeof(toast_msg) - 1) {
		toast_msg[i] = msg[i];
		i++;
	}
	toast_msg[i] = 0;
	toast_timer = duration;
}

internal void
spawn_particles(float x, float y, u32 color, int count) {
	int spawned = 0;
	for (int i = 0; i < MAX_PARTICLES && spawned < count; i++) {
		if (particles[i].life <= 0.f) {
			float angle = (float)((spawned * 137 + i * 29) % 360) * 0.0174533f;
			float speed = 18.f + (float)((i * 17) % 38);
			particles[i].x = x;
			particles[i].y = y;
			particles[i].vx = cosf(angle) * speed;
			particles[i].vy = sinf(angle) * speed;
			particles[i].max_life = 0.55f;
			particles[i].life = 0.55f;
			particles[i].color = color;
			spawned++;
		}
	}
}

internal bool
point_in_rect(float px, float py, float cx, float cy, float hw, float hh) {
	return absf(px - cx) <= hw && absf(py - cy) <= hh;
}

internal bool
str_eq_ci(const char* a, const char* b) {
	int i = 0;
	while (a[i] && b[i]) {
		char ca = a[i], cb = b[i];
		if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
		if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
		if (ca != cb) return false;
		i++;
	}
	return a[i] == b[i];
}

internal void
draw_game_icon(const Game_Entry* entry, float cx, float cy, u32 accent) {
	draw_rect(cx, cy, 5.8f, 5.8f, 0x050814);
	if (str_eq_ci(entry->icon, "PONG")) {
		float bob = sinf(anim_time * 3.5f) * 1.6f;
		draw_rect(cx, cy, 0.22f, 4.2f, 0x334155);
		draw_rect(cx - 3.8f, cy + bob * 0.6f, 0.65f, 2.0f, accent);
		draw_rect(cx + 3.8f, cy - bob * 0.6f, 0.65f, 2.0f, 0xF59E0B);
		draw_rect(cx + sinf(anim_time * 2.2f) * 2.0f, cy + bob, 0.75f, 0.75f, 0xF8FAFC);
	} else {
		draw_rect(cx, cy, 3.4f, 4.0f, accent);
		draw_rect(cx, cy + 1.1f, 2.2f, 1.5f, 0x050814);
		draw_rect(cx - 1.4f, cy - 2.4f, 0.45f, 0.8f, 0x050814);
		draw_rect(cx,        cy - 2.4f, 0.45f, 0.8f, 0x050814);
		draw_rect(cx + 1.4f, cy - 2.4f, 0.45f, 0.8f, 0x050814);
	}
}

internal void
draw_pill(float cx, float cy, float hw, float hh, const char* label, u32 bg, u32 border, u32 fg, bool hovered) {
	draw_rect(cx, cy, hw + 0.3f, hh + 0.3f, hovered ? 0xF8FAFC : border);
	draw_rect(cx, cy, hw, hh, bg);
	draw_text_centered(label, cx, cy, 0.30f, fg);
}

void
init_launcher() {
	u32 rng = 4242u;
	for (int i = 0; i < MAX_STARS; i++) {
		rng = rng * 1664525u + 1013904223u;
		stars[i].x = (float)((s32)(rng % 170) - 85);
		rng = rng * 1664525u + 1013904223u;
		stars[i].y = (float)((s32)(rng % 94) - 47);
		stars[i].speed = 2.5f + (float)(i % 5) * 1.8f;
		stars[i].size = (i % 3 == 0) ? 0.38f : 0.24f;
	}
	for (int i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0.f;

	load_catalog(&catalog, "games.cfg");
	selected_idx = 0;
	scroll_offset = 0;
	launcher_state = LAUNCHER_SPLASH;
	splash_timer = 0.f;
	splash_stage_beeped = 0;
	set_bgm_enabled(false);
}

internal void
enter_menu() {
	launcher_state = LAUNCHER_MENU;
	set_bgm_enabled(bgm_on);
}

internal void
trigger_selected_launch() {
	if (catalog.count <= 0) return;
	const Game_Entry* entry = &catalog.entries[selected_idx];
	if (entry->available) {
		launcher_state = LAUNCHER_LAUNCHING;
		launch_timer = 0.32f;
		sfx(440, 200, 0.26f, 1180.f);
		spawn_particles(0.f, 0.f, entry->accent, 28);
	} else {
		sfx(180, 160, 0.25f, 110.f);
		show_toast("EXECUTABLE NOT FOUND");
	}
}

void
simulate_launcher(Input* input, float dt) {
	anim_time += dt;
	if (toast_timer > 0.f) toast_timer -= dt;

	for (int i = 0; i < MAX_PARTICLES; i++) {
		if (particles[i].life > 0.f) {
			particles[i].x += particles[i].vx * dt;
			particles[i].y += particles[i].vy * dt;
			particles[i].life -= dt;
		}
	}

	for (int i = 0; i < MAX_STARS; i++) {
		stars[i].y -= stars[i].speed * dt;
		if (stars[i].y < -48.f) stars[i].y = 48.f;
	}

	const Theme& th = THEMES[theme_idx];

	// -----------------------------------------------------------------------
	// SPLASH SCREEN (Nostalgic Boot + Logo Reveal)
	// -----------------------------------------------------------------------
	if (launcher_state == LAUNCHER_SPLASH) {
		splash_timer += dt;

		if (splash_stage_beeped == 0) {
			splash_stage_beeped = 1;
			sfx(110, 260, 0.22f, 880.f);
		} else if (splash_stage_beeped == 1 && splash_timer >= 0.55f) {
			splash_stage_beeped = 2;
			sfx(740, 35, 0.14f);
		} else if (splash_stage_beeped == 2 && splash_timer >= 1.05f) {
			splash_stage_beeped = 3;
			sfx(740, 35, 0.14f);
		} else if (splash_stage_beeped == 3 && splash_timer >= 1.55f) {
			splash_stage_beeped = 4;
			sfx(880, 45, 0.16f);
		} else if (splash_stage_beeped == 4 && splash_timer >= 2.05f) {
			splash_stage_beeped = 5;
			sfx(523, 120, 0.22f, 1046.f);
			spawn_particles(0.f, 8.f, th.accent, 24);
		}

		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE) ||
		    pressed(BUTTON_ESC) || pressed(BUTTON_MOUSE_LEFT)) {
			sfx(880, 55, 0.18f);
			enter_menu();
			return;
		}
		if (splash_timer >= 3.4f) {
			enter_menu();
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
				if (splash_timer >= 0.55f)
					draw_text_left("MEMORY CHECK ..... OK", -64.f, 12.f, 0.40f, th.text);
				if (splash_timer >= 1.05f)
					draw_text_left("AUDIO SYNTH ...... READY", -64.f, 4.f, 0.40f, th.text);
				if (splash_timer >= 1.55f) {
					draw_text_left("GAMES DETECTED ... ", -64.f, -4.f, 0.40f, 0x22C55E);
					draw_number(catalog.count, 10.f, -4.f, 0.40f, th.accent);
				}
				if (((int)(splash_timer * 6.f)) % 2 == 0) {
					draw_rect(-62.f, -13.f, 1.4f, 2.0f, th.accent);
				}
			} else {
				for (int i = 0; i < MAX_STARS; i++) {
					draw_rect_alpha(stars[i].x, stars[i].y, stars[i].size, stars[i].size, th.accent, 0.40f);
				}
				for (int i = 0; i < MAX_PARTICLES; i++) {
					if (particles[i].life > 0.f) {
						float a = particles[i].life / particles[i].max_life;
						draw_rect_alpha(particles[i].x, particles[i].y, 0.7f, 0.7f, particles[i].color, a);
					}
				}
				draw_text_centered("ARCADE HUB", 0.f, 8.f, 1.40f, th.accent);
				draw_text_centered("SYSTEM READY", 0.f, -10.f, 0.45f, th.text);
			}
			draw_text_centered("PRESS SPACE TO SKIP", 0.f, -39.f, 0.32f, th.text_dim);
		}
		return;
	}

	// -----------------------------------------------------------------------
	// LAUNCHING TRANSITION
	// -----------------------------------------------------------------------
	if (launcher_state == LAUNCHER_LAUNCHING) {
		launch_timer -= dt;
		clear_screen(th.bg_outer);
		draw_rect(0.f, 0.f, 85.f, 45.f, th.panel_bg);
		for (int i = 0; i < MAX_PARTICLES; i++) {
			if (particles[i].life > 0.f) {
				float a = particles[i].life / particles[i].max_life;
				draw_rect_alpha(particles[i].x, particles[i].y, 0.8f, 0.8f, particles[i].color, a);
			}
		}
		draw_text_centered("LAUNCHING...", 0.f, 2.f, 0.75f, th.accent);

		if (launch_timer <= 0.f) {
			const Game_Entry* entry = &catalog.entries[selected_idx];
			bool ok = launch_game(entry->exe, entry->workdir);
			refresh_catalog_availability(&catalog);
			launcher_state = LAUNCHER_MENU;
			if (!ok) {
				sfx(180, 160, 0.25f, 110.f);
				show_toast("FAILED TO LAUNCH GAME");
			}
		}
		return;
	}

	// -----------------------------------------------------------------------
	// MAIN MENU
	// -----------------------------------------------------------------------
	if (pressed(BUTTON_ESC)) {
		request_quit();
		return;
	}
	if (pressed(BUTTON_M)) {
		sound_muted = !sound_muted;
		set_audio_muted(sound_muted);
		if (!sound_muted) sfx(580, 45, 0.18f);
	}
	if (pressed(BUTTON_B)) {
		bgm_on = !bgm_on;
		set_bgm_enabled(bgm_on);
		sfx(520, 40, 0.16f);
	}
	if (pressed(BUTTON_T) || pressed(BUTTON_RIGHT)) {
		theme_idx = (theme_idx + 1) % 3;
		sfx(520, 35, 0.15f);
	}
	if (pressed(BUTTON_LEFT)) {
		theme_idx = (theme_idx + 2) % 3;
		sfx(520, 35, 0.15f);
	}

	float mx = input->mouse_x;
	float my = input->mouse_y;
	bool hov_theme = point_in_rect(mx, my, 40.f, 36.f, 11.f, 2.6f);
	bool hov_music = point_in_rect(mx, my, 64.f, 36.f, 11.f, 2.6f);

	if (pressed(BUTTON_MOUSE_LEFT)) {
		if (hov_theme) {
			theme_idx = (theme_idx + 1) % 3;
			sfx(520, 35, 0.15f);
		} else if (hov_music) {
			bgm_on = !bgm_on;
			set_bgm_enabled(bgm_on);
			sfx(520, 40, 0.16f);
		}
	}

	const int VISIBLE_CARDS = 4;
	if (catalog.count > 0) {
		if (pressed(BUTTON_UP) || pressed(BUTTON_W)) {
			selected_idx = (selected_idx + catalog.count - 1) % catalog.count;
			sfx(440, 35, 0.15f);
		}
		if (pressed(BUTTON_DOWN) || pressed(BUTTON_S)) {
			selected_idx = (selected_idx + 1) % catalog.count;
			sfx(440, 35, 0.15f);
		}

		int end = scroll_offset + VISIBLE_CARDS;
		if (end > catalog.count) end = catalog.count;
		for (int i = scroll_offset; i < end; i++) {
			int slot = i - scroll_offset;
			float cy = 17.f - slot * 14.5f;
			if (point_in_rect(mx, my, 0.f, cy, 74.f, 6.2f)) {
				if (input->mouse_moved && selected_idx != i) {
					selected_idx = i;
					sfx(440, 28, 0.12f);
				}
				if (pressed(BUTTON_MOUSE_LEFT)) {
					selected_idx = i;
					trigger_selected_launch();
					return;
				}
			}
		}

		if (selected_idx < scroll_offset) scroll_offset = selected_idx;
		if (selected_idx >= scroll_offset + VISIBLE_CARDS) {
			scroll_offset = selected_idx - VISIBLE_CARDS + 1;
		}

		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE)) {
			trigger_selected_launch();
			return;
		}
	}

	// Render frame
	clear_screen(th.bg_outer);
	draw_rect(0.f, 0.f, 86.2f, 46.2f, th.panel_border);
	draw_rect(0.f, 0.f, 85.0f, 45.0f, th.panel_bg);

	for (int i = 0; i < MAX_STARS; i++) {
		draw_rect_alpha(stars[i].x, stars[i].y, stars[i].size, stars[i].size, th.accent, 0.30f);
	}

	// Clean header bar
	draw_text_left("ARCADE HUB", -74.f, 36.f, 0.85f, th.accent);
	draw_rect(-0.f, 29.5f, 74.f, 0.22f, th.panel_border);

	draw_pill(40.f, 36.f, 11.f, 2.6f, th.name, th.card_bg, th.panel_border, th.text, hov_theme);
	const char* music_lbl = sound_muted ? "MUTED" : (bgm_on ? "MUSIC:ON" : "MUSIC:OFF");
	u32 music_col = sound_muted ? 0xEF4444 : (bgm_on ? 0x22C55E : th.text_dim);
	draw_pill(64.f, 36.f, 11.f, 2.6f, music_lbl, th.card_bg, th.panel_border, music_col, hov_music);

	// Game cards
	if (catalog.count == 0) {
		draw_rect(0.f, 0.f, 68.f, 12.f, th.card_bg);
		draw_text_centered("NO GAMES FOUND IN GAMES.CFG", 0.f, 2.f, 0.50f, 0xEF4444);
		draw_text_centered("ADD A SECTION TO LAUNCHER/GAMES.CFG", 0.f, -5.f, 0.38f, th.text_dim);
	} else {
		int end = scroll_offset + VISIBLE_CARDS;
		if (end > catalog.count) end = catalog.count;

		for (int i = scroll_offset; i < end; i++) {
			int slot = i - scroll_offset;
			float cy = 17.f - slot * 14.5f;
			const Game_Entry* e = &catalog.entries[i];
			bool is_sel = (i == selected_idx);

			u32 border_col = is_sel ? th.accent : th.panel_border;
			u32 bg_col     = is_sel ? th.card_selected : th.card_bg;
			draw_rect(0.f, cy, 74.6f, 6.4f, border_col);
			draw_rect(0.f, cy, 74.0f, 5.8f, bg_col);

			if (is_sel) {
				float pulse = 0.5f * sinf(anim_time * 6.f);
				draw_text_centered(">", -78.f + pulse, cy, 0.58f, th.accent);
			}

			draw_game_icon(e, -64.f, cy, e->accent);
			draw_text_left(e->title, -53.f, cy + 2.1f, 0.52f, is_sel ? th.text : th.text_dim);
			draw_text_left(e->subtitle, -53.f, cy - 2.2f, 0.30f, th.text_dim);

			draw_text_centered(e->players, 42.f, cy + 1.9f, 0.34f, th.text_dim);
			const char* status = e->available ? (is_sel ? "PLAY" : "READY") : "MISSING";
			u32 status_col = e->available ? (is_sel ? th.accent : 0x22C55E) : 0xEF4444;
			draw_text_centered(status, 62.f, cy - 1.9f, 0.36f, status_col);
		}
	}

	if (toast_timer > 0.f) {
		draw_rect(0.f, -34.f, 42.f, 2.8f, 0x7F1D1D);
		draw_text_centered(toast_msg, 0.f, -34.f, 0.36f, 0xFCA5A5);
	}

	draw_text_centered("UP/DOWN: SELECT   ENTER/CLICK: PLAY   B: MUSIC   T: THEME   M: MUTE   ESC: QUIT",
	                   0.f, -40.5f, 0.30f, th.text_dim);
}

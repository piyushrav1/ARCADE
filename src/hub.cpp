#include <math.h>
#include "utils.h"
#include "renderer.h"
#include "keybinds.h"
#include "audio_ui.h"
#include "save.h"
#include "game_interface.h"

#define pressed(b) (input->buttons[b].is_down && input->buttons[b].changed)

enum Hub_State {
	HUB_SPLASH,
	HUB_PROFILE,
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
	int id;
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

global_variable const char* AVATAR_NAMES[6] = {
	"VISOR", "BOT", "ACE", "PHANTOM", "NINJA", "CHAMP"
};

struct Star {
	float x, y, speed, size;
};

struct Particle {
	float x, y, vx, vy, life, max_life;
	u32 color;
};

#define HUB_MAX_STARS     40
#define HUB_MAX_PARTICLES 48
global_variable Star stars[HUB_MAX_STARS];
global_variable Particle particles[HUB_MAX_PARTICLES];

global_variable Hub_State hub_state = HUB_SPLASH;
global_variable Active_Scene active_scene = SCENE_HUB;
global_variable float splash_timer = 0.f;
global_variable int   splash_stage = 0;

global_variable int  selected_idx = 0;
global_variable int  controls_tab = 0;
global_variable int  theme_idx = 0;
global_variable bool bgm_on = true;
global_variable float anim_time = 0.f;

// Profile editor state
global_variable char edit_name[16] = "PLAYER 1";
global_variable int  edit_name_len = 8;
global_variable int  edit_avatar = 0;

internal void
sfx(int freq_hz, int duration_ms, float volume = 0.22f, float freq_end_hz = 0.f) {
	if (!is_audio_muted()) play_tone(freq_hz, duration_ms, volume, freq_end_hz);
}

internal bool
point_in_rect(float px, float py, float cx, float cy, float hw, float hh) {
	return absf(px - cx) <= hw && absf(py - cy) <= hh;
}

internal void
spawn_particles(float x, float y, u32 color, int count) {
	int spawned = 0;
	for (int i = 0; i < HUB_MAX_PARTICLES && spawned < count; i++) {
		if (particles[i].life <= 0.f) {
			float angle = (float)((spawned * 137 + i * 29) % 360) * 0.0174533f;
			float speed = 18.f + (float)((i * 19) % 42);
			particles[i].x = x;
			particles[i].y = y;
			particles[i].vx = cosf(angle) * speed;
			particles[i].vy = sinf(angle) * speed;
			particles[i].max_life = 0.60f;
			particles[i].life = 0.60f;
			particles[i].color = color;
			spawned++;
		}
	}
}

internal void
draw_avatar(int id, float cx, float cy, float s) {
	draw_rect(cx, cy, 4.6f * s, 4.6f * s, 0x050814);
	if (id == 0) {
		// VISOR: cyber helmet + glowing cyan visor
		draw_rect(cx, cy + 0.3f * s, 3.2f * s, 3.0f * s, 0x334155);
		draw_rect(cx, cy + 0.5f * s, 2.6f * s, 0.9f * s, 0x22D3EE);
		draw_rect(cx, cy - 1.6f * s, 2.0f * s, 0.8f * s, 0x475569);
	} else if (id == 1) {
		// BOT: retro robot + antenna
		draw_rect(cx, cy + 3.2f * s, 0.4f * s, 0.9f * s, 0x94A3B8);
		draw_rect(cx, cy - 0.2f * s, 3.0f * s, 2.8f * s, 0x22C55E);
		draw_rect(cx - 1.1f * s, cy + 0.5f * s, 0.6f * s, 0.6f * s, 0xFACC15);
		draw_rect(cx + 1.1f * s, cy + 0.5f * s, 0.6f * s, 0.6f * s, 0xFACC15);
		draw_rect(cx, cy - 1.4f * s, 1.6f * s, 0.4f * s, 0x050814);
	} else if (id == 2) {
		// ACE: pixel starfighter
		draw_rect(cx, cy + 0.4f * s, 0.9f * s, 2.8f * s, 0x38BDF8);
		draw_rect(cx, cy - 0.8f * s, 3.2f * s, 1.0f * s, 0x0284C7);
		draw_rect(cx, cy + 1.2f * s, 0.5f * s, 0.8f * s, 0xF8FAFC);
		draw_rect(cx, cy - 2.6f * s, 0.7f * s, 0.7f * s, 0xF59E0B);
	} else if (id == 3) {
		// PHANTOM: arcade ghost
		draw_rect(cx, cy + 0.4f * s, 2.8f * s, 2.6f * s, 0xEC4899);
		draw_rect(cx - 1.6f * s, cy - 2.4f * s, 0.8f * s, 0.7f * s, 0xEC4899);
		draw_rect(cx,            cy - 2.4f * s, 0.8f * s, 0.7f * s, 0xEC4899);
		draw_rect(cx + 1.6f * s, cy - 2.4f * s, 0.8f * s, 0.7f * s, 0xEC4899);
		draw_rect(cx - 1.0f * s, cy + 0.7f * s, 0.6f * s, 0.7f * s, 0xF8FAFC);
		draw_rect(cx + 1.0f * s, cy + 0.7f * s, 0.6f * s, 0.7f * s, 0xF8FAFC);
	} else if (id == 4) {
		// NINJA: dark hood + crimson headband
		draw_rect(cx, cy, 3.0f * s, 3.0f * s, 0x1E293B);
		draw_rect(cx, cy + 1.4f * s, 3.2f * s, 0.7f * s, 0xEF4444);
		draw_rect(cx, cy + 0.2f * s, 2.2f * s, 0.7f * s, 0xFED7AA);
		draw_rect(cx - 0.9f * s, cy + 0.2f * s, 0.4f * s, 0.4f * s, 0x050814);
		draw_rect(cx + 0.9f * s, cy + 0.2f * s, 0.4f * s, 0.4f * s, 0x050814);
	} else {
		// CHAMP: golden crown
		draw_rect(cx, cy - 1.2f * s, 3.2f * s, 1.4f * s, 0xFACC15);
		draw_rect(cx - 2.2f * s, cy + 1.0f * s, 0.8f * s, 1.4f * s, 0xFACC15);
		draw_rect(cx,            cy + 1.4f * s, 0.8f * s, 1.8f * s, 0xFACC15);
		draw_rect(cx + 2.2f * s, cy + 1.0f * s, 0.8f * s, 1.4f * s, 0xFACC15);
		draw_rect(cx, cy - 1.2f * s, 0.7f * s, 0.7f * s, 0xEF4444);
	}
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
open_profile_editor() {
	int i = 0;
	while (user_stats.player_name[i] && i < 12) {
		edit_name[i] = user_stats.player_name[i];
		i++;
	}
	edit_name[i] = 0;
	edit_name_len = i;
	edit_avatar = user_stats.avatar_id;
	hub_state = HUB_PROFILE;
	set_bgm_enabled(false);
}

internal void
save_profile_and_enter_menu() {
	while (edit_name_len > 0 && edit_name[edit_name_len - 1] == ' ') {
		edit_name[--edit_name_len] = 0;
	}
	if (edit_name_len == 0) {
		const char* def = "PLAYER 1";
		for (int i = 0; def[i]; i++) edit_name[i] = def[i];
		edit_name[8] = 0;
	}
	for (int i = 0; i < 16; i++) user_stats.player_name[i] = edit_name[i];
	user_stats.avatar_id = edit_avatar;
	user_stats.profile_created = true;
	save_user_data();
	hub_state = HUB_MENU;
	set_bgm_enabled(bgm_on);
	sfx(660, 110, 0.22f, 990.f);
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

internal void
draw_hub_backdrop(const Theme& th) {
	clear_screen(th.bg_outer);
	draw_rect(0.f, 0.f, 86.2f, 46.2f, th.panel_border);
	draw_rect(0.f, 0.f, 85.0f, 45.0f, th.panel_bg);

	for (int i = 0; i < HUB_MAX_STARS; i++) {
		draw_rect_alpha(stars[i].x, stars[i].y, stars[i].size, stars[i].size, th.accent, 0.30f);
	}

	draw_text_left("ARCADE HUB", -74.f, 36.f, 0.85f, th.accent);

	// Player profile badge
	draw_avatar(user_stats.avatar_id, -70.f, 25.5f, 0.55f);
	draw_text_left(user_stats.player_name, -63.f, 25.5f, 0.36f, th.text);
	draw_text_left("(F2)", -63.f + 12 * 2.4f, 25.5f, 0.28f, th.text_dim);

	draw_rect(0.f, 20.5f, 74.f, 0.22f, th.panel_border);
}

void
hub_init() {
	load_user_data();
	theme_idx = user_stats.hub_theme;
	bgm_on = user_stats.hub_bgm_on;

	u32 rng = 4242u;
	for (int i = 0; i < HUB_MAX_STARS; i++) {
		rng = rng * 1664525u + 1013904223u;
		stars[i].x = (float)((s32)(rng % 170) - 85);
		rng = rng * 1664525u + 1013904223u;
		stars[i].y = (float)((s32)(rng % 94) - 47);
		stars[i].speed = 2.5f + (float)(i % 5) * 1.8f;
		stars[i].size = (i % 3 == 0) ? 0.38f : 0.24f;
	}
	for (int i = 0; i < HUB_MAX_PARTICLES; i++) particles[i].life = 0.f;

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
		float mult = (hub_state == HUB_SPLASH && splash_timer < 1.2f) ? 5.5f : 1.0f;
		stars[i].y -= stars[i].speed * mult * dt;
		if (stars[i].y < -48.f) stars[i].y = 48.f;
	}
	for (int i = 0; i < HUB_MAX_PARTICLES; i++) {
		if (particles[i].life > 0.f) {
			particles[i].x += particles[i].vx * dt;
			particles[i].y += particles[i].vy * dt;
			particles[i].life -= dt;
		}
	}

	const Theme& th = THEMES[theme_idx];

	// -----------------------------------------------------------------------
	// CREATIVE SPLASH SCREEN (Neon Warp Tunnel + Converging Sprites + Logo)
	// -----------------------------------------------------------------------
	if (hub_state == HUB_SPLASH) {
		splash_timer += dt;

		if (splash_stage == 0) { splash_stage = 1; sfx(140, 320, 0.22f, 660.f); }
		else if (splash_stage == 1 && splash_timer >= 0.90f) {
			splash_stage = 2;
			sfx(523, 160, 0.24f, 1046.f);
			spawn_particles(0.f, 4.f, th.accent, 32);
		}

		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE) ||
		    pressed(BUTTON_ESC) || pressed(BUTTON_MOUSE_LEFT) || splash_timer >= 3.2f) {
			if (!user_stats.profile_created) open_profile_editor();
			else { hub_state = HUB_MENU; set_bgm_enabled(bgm_on); }
			return;
		}

		clear_screen(0x020408);
		draw_rect(0.f, 0.f, 86.2f, 46.2f, th.panel_border);
		draw_rect(0.f, 0.f, 85.0f, 45.0f, th.panel_bg);

		for (int i = 0; i < HUB_MAX_STARS; i++) {
			draw_rect_alpha(stars[i].x, stars[i].y, stars[i].size, stars[i].size * 1.8f, th.accent, 0.50f);
		}

		// Neon warp tunnel rings
		for (int r = 0; r < 5; r++) {
			float phase = fmodf(splash_timer * 1.4f + r * 0.2f, 1.0f);
			float rw = 10.f + phase * 74.f;
			float rh = 5.f + phase * 39.f;
			draw_rect_alpha(0.f, 0.f, rw, rh, th.accent, (1.f - phase) * 0.14f);
		}

		if (splash_timer >= 0.90f) {
			float t = clampf(0.f, (splash_timer - 0.90f) / 0.55f, 1.f);
			float offset = (1.f - t) * 55.f;
			// Converging Pong & Breaker sprites
			draw_rect(-46.f - offset, 4.f, 1.2f, 5.f, 0x38BDF8);
			draw_rect( 46.f + offset, 4.f, 1.2f, 5.f, 0xF59E0B);
			draw_rect(-22.f - offset, -16.f, 4.f, 1.4f, 0xA855F7);
			draw_rect( 22.f + offset, -16.f, 4.f, 1.4f, 0xEF4444);

			for (int i = 0; i < HUB_MAX_PARTICLES; i++) {
				if (particles[i].life > 0.f) {
					float a = particles[i].life / particles[i].max_life;
					draw_rect_alpha(particles[i].x, particles[i].y, 0.75f, 0.75f, particles[i].color, a);
				}
			}

			draw_text_centered("ARCADE HUB", 0.f, 6.f, 1.35f, th.accent);
			draw_rect(0.f, -6.f, t * 44.f, 0.35f, 0xFACC15);
			if (splash_timer >= 1.8f) {
				draw_text_centered("PRESS SPACE TO CONTINUE", 0.f, -26.f, 0.36f, th.text);
			}
		} else {
			draw_text_centered("INITIALIZING...", 0.f, 0.f, 0.50f, th.text_dim);
		}
		return;
	}

	// -----------------------------------------------------------------------
	// PROFILE SETUP POPUP (Post-Splash or via F2 / Profile Badge)
	// -----------------------------------------------------------------------
	if (hub_state == HUB_PROFILE) {
		for (int i = 0; i < input->text_input_len; i++) {
			char ch = input->text_input[i];
			if (ch == '\b') {
				if (edit_name_len > 0) {
					edit_name[--edit_name_len] = 0;
					sfx(360, 25, 0.12f);
				}
			} else {
				if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
				bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
				          ch == ' ' || ch == '-' || ch == '!';
				if (ok && edit_name_len < 12) {
					edit_name[edit_name_len++] = ch;
					edit_name[edit_name_len] = 0;
					sfx(620, 22, 0.12f);
				}
			}
		}

		if (vk_pressed(input, VK_LEFT))  { edit_avatar = (edit_avatar + 5) % 6; sfx(480, 35, 0.14f); }
		if (vk_pressed(input, VK_RIGHT)) { edit_avatar = (edit_avatar + 1) % 6; sfx(480, 35, 0.14f); }
		if (vk_pressed(input, VK_RETURN)) { save_profile_and_enter_menu(); return; }
		if (vk_pressed(input, VK_ESCAPE)) {
			if (user_stats.profile_created) {
				hub_state = HUB_MENU;
				set_bgm_enabled(bgm_on);
				sfx(400, 40, 0.14f);
			} else {
				save_profile_and_enter_menu();
			}
			return;
		}

		float mx = input->mouse_x;
		float my = input->mouse_y;
		if (vk_pressed(input, VK_LBUTTON)) {
			for (int a = 0; a < 6; a++) {
				float ax = -40.f + a * 16.f;
				if (point_in_rect(mx, my, ax, 10.f, 6.2f, 6.2f)) {
					edit_avatar = a;
					sfx(520, 35, 0.15f);
				}
			}
			if (point_in_rect(mx, my, 0.f, -24.f, 22.f, 3.6f)) {
				save_profile_and_enter_menu();
				return;
			}
		}

		draw_hub_backdrop(th);
		draw_rect_alpha(0.f, 0.f, 85.f, 45.f, 0x020408, 0.82f);

		// Modal popup box
		draw_rect(0.f, 0.f, 58.6f, 34.6f, th.accent);
		draw_rect(0.f, 0.f, 58.0f, 34.0f, th.panel_bg);

		draw_text_centered(user_stats.profile_created ? "EDIT PROFILE" : "CREATE YOUR PROFILE",
		                   0.f, 26.f, 0.65f, th.accent);

		// 6 Avatar selector boxes
		for (int a = 0; a < 6; a++) {
			float ax = -40.f + a * 16.f;
			bool sel = (a == edit_avatar);
			draw_rect(ax, 10.f, 6.2f, 6.2f, sel ? 0xFACC15 : th.panel_border);
			draw_avatar(a, ax, 10.f, 1.05f);
		}
		draw_text_centered(AVATAR_NAMES[edit_avatar], 0.f, 0.5f, 0.36f, 0xFACC15);

		// Name input field
		draw_text_centered("PLAYER NAME:", 0.f, -6.f, 0.34f, th.text_dim);
		draw_rect(0.f, -13.f, 36.4f, 3.6f, th.accent);
		draw_rect(0.f, -13.f, 36.0f, 3.2f, 0x050814);

		char display_name[20];
		int di = 0;
		for (int i = 0; edit_name[i] && di < 16; i++) display_name[di++] = edit_name[i];
		if (((int)(anim_time * 3.f)) % 2 == 0) display_name[di++] = '-';
		display_name[di] = 0;
		draw_text_centered(display_name, 0.f, -13.f, 0.44f, th.text);

		bool hov_save = point_in_rect(mx, my, 0.f, -24.f, 22.f, 3.6f);
		draw_pill(0.f, -24.f, 22.f, 3.6f, "SAVE AND PLAY", th.card_selected, th.accent, 0x22C55E, hov_save);

		draw_text_centered("TYPE NAME   LEFT/RIGHT: AVATAR   ENTER: SAVE", 0.f, -38.f, 0.30f, th.text_dim);
		return;
	}

	// HUB_CONTROLS
	if (hub_state == HUB_CONTROLS) {
		if (update_and_draw_keybinds_ui(input, &controls_tab, true,
		                                th.bg_outer, th.panel_bg, th.panel_border,
		                                th.card_bg, th.text, th.text_dim, th.accent)) {
			save_user_data();
			hub_state = HUB_MENU;
		}
		return;
	}

	// HUB_SOUND
	if (hub_state == HUB_SOUND) {
		if (update_and_draw_audio_ui(input,
		                             th.bg_outer, th.panel_bg, th.panel_border,
		                             th.card_bg, th.text, th.text_dim, th.accent)) {
			save_user_data();
			hub_state = HUB_MENU;
		}
		return;
	}

	// HUB_MENU
	if (pressed(BUTTON_ESC)) { request_quit(); return; }
	if (vk_pressed(input, VK_F2)) { open_profile_editor(); sfx(520, 40, 0.16f); return; }
	if (vk_pressed(input, VK_TAB)) {
		controls_tab = selected_idx;
		hub_state = HUB_CONTROLS;
		sfx(520, 40, 0.16f);
		return;
	}
	if (pressed(BUTTON_M)) {
		set_audio_muted(!is_audio_muted());
		if (!is_audio_muted()) sfx(580, 45, 0.18f);
		save_user_data();
	}
	if (pressed(BUTTON_B)) {
		bgm_on = !bgm_on;
		user_stats.hub_bgm_on = bgm_on;
		set_bgm_enabled(bgm_on);
		save_user_data();
		sfx(520, 40, 0.16f);
	}
	if (pressed(BUTTON_T) || pressed(BUTTON_RIGHT)) { theme_idx = (theme_idx + 1) % 3; user_stats.hub_theme = theme_idx; save_user_data(); sfx(520, 35, 0.15f); }
	if (pressed(BUTTON_LEFT))                         { theme_idx = (theme_idx + 2) % 3; user_stats.hub_theme = theme_idx; save_user_data(); sfx(520, 35, 0.15f); }
	if (pressed(BUTTON_UP) || pressed(BUTTON_W))      { selected_idx = (selected_idx + NUM_GAMES - 1) % NUM_GAMES; sfx(440, 35, 0.15f); }
	if (pressed(BUTTON_DOWN) || pressed(BUTTON_S))    { selected_idx = (selected_idx + 1) % NUM_GAMES; sfx(440, 35, 0.15f); }

	float mx = input->mouse_x;
	float my = input->mouse_y;
	bool hov_prof  = point_in_rect(mx, my, -56.f, 25.5f, 20.f, 3.2f);
	bool hov_keys  = point_in_rect(mx, my, -5.f, 36.f, 10.f, 2.6f);
	bool hov_sound = point_in_rect(mx, my, 18.f, 36.f, 10.f, 2.6f);
	bool hov_theme = point_in_rect(mx, my, 41.f, 36.f, 10.f, 2.6f);
	bool hov_music = point_in_rect(mx, my, 64.f, 36.f, 10.f, 2.6f);

	if (pressed(BUTTON_MOUSE_LEFT)) {
		if (hov_prof)       { open_profile_editor(); sfx(520, 40, 0.16f); return; }
		else if (hov_keys)  { controls_tab = selected_idx; hub_state = HUB_CONTROLS; sfx(520, 40, 0.16f); return; }
		else if (hov_sound) { hub_state = HUB_SOUND; sfx(520, 40, 0.16f); return; }
		else if (hov_theme) { theme_idx = (theme_idx + 1) % 3; user_stats.hub_theme = theme_idx; save_user_data(); sfx(520, 35, 0.15f); }
		else if (hov_music) { bgm_on = !bgm_on; user_stats.hub_bgm_on = bgm_on; set_bgm_enabled(bgm_on); save_user_data(); sfx(520, 40, 0.16f); }
	}

	for (int i = 0; i < NUM_GAMES; i++) {
		float cy = 10.f - i * 16.5f;
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

	draw_hub_backdrop(th);

	draw_pill(-5.f, 36.f, 10.f, 2.6f, "KEYS",  th.card_bg, th.panel_border, th.text, hov_keys);
	draw_pill(18.f, 36.f, 10.f, 2.6f, "SOUND", th.card_bg, th.panel_border, th.text, hov_sound);
	draw_pill(41.f, 36.f, 10.f, 2.6f, th.name, th.card_bg, th.panel_border, th.text, hov_theme);
	bool muted = is_audio_muted();
	const char* music_lbl = muted ? "MUTED" : (bgm_on ? "MUSIC:ON" : "MUSIC:OFF");
	u32 music_col = muted ? 0xEF4444 : (bgm_on ? 0x22C55E : th.text_dim);
	draw_pill(64.f, 36.f, 10.f, 2.6f, music_lbl, th.card_bg, th.panel_border, music_col, hov_music);

	for (int i = 0; i < NUM_GAMES; i++) {
		float cy = 10.f - i * 16.5f;
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

	draw_text_centered("ENTER/CLICK: PLAY   F2: PROFILE   TAB: KEYS   B: MUSIC   T: THEME   ESC: QUIT",
	                   0.f, -40.5f, 0.27f, th.text_dim);
}

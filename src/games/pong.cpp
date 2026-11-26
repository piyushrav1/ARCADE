#include <math.h>
#include "utils.h"
#include "renderer.h"
#include "keybinds.h"
#include "audio_ui.h"
#include "graphics_ui.h"
#include "themes.h"
#include "save.h"
#include "game_interface.h"

#define pressed(b) (input->buttons[b].is_down && input->buttons[b].changed)

namespace Pong {

enum Game_State {
	STATE_INTRO,
	STATE_MENU,
	STATE_CONTROLS,
	STATE_SOUND,
	STATE_GRAPHICS,
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_GAME_OVER
};

enum Difficulty {
	DIFF_EASY = 0,
	DIFF_MEDIUM = 1,
	DIFF_HARD = 2
};

enum Powerup_Type {
	PWR_NONE = 0,
	PWR_BIG_PADDLE = 1,
	PWR_MULTIBALL = 2,
	PWR_SPEED = 3
};

struct Ball {
	bool active;
	float x, y;
	float dx, dy;
	float speed_base;
};

struct Trail_Point {
	float x, y;
};

struct Particle {
	float x, y;
	float vx, vy;
	float life;
	float max_life;
	u32 color;
};

global_variable const float arena_half_size_x = 85.f;
global_variable const float arena_half_size_y = 45.f;
global_variable const float player_half_size_x = 2.2f;
global_variable const float base_paddle_half_y = 11.f;
global_variable const float ball_half_size = 1.2f;
global_variable const int   WIN_SCORES[3] = { 5, 7, 11 };

global_variable Game_State game_state = STATE_INTRO;
global_variable float intro_timer = 0.f;
global_variable int   intro_stage = 0;

global_variable bool is_single_player = true;
global_variable Difficulty difficulty = DIFF_MEDIUM;
global_variable bool arcade_mode = true;
global_variable int win_score_idx = 1;
global_variable bool show_perf_hud = false;

global_variable int menu_selection = 0;
global_variable int pause_selection = 0;
global_variable int gameover_selection = 0;
global_variable Game_State controls_return_state = STATE_MENU;

global_variable float player_1_p = 0.f, player_1_dp = 0.f;
global_variable float player_2_p = 0.f, player_2_dp = 0.f;
global_variable float p1_half_y = base_paddle_half_y, p2_half_y = base_paddle_half_y;
global_variable float p1_big_timer = 0.f, p2_big_timer = 0.f;
global_variable float p1_flash = 0.f, p2_flash = 0.f;
global_variable int   last_hitter = 1;

#define PONG_MAX_BALLS 2
global_variable Ball balls[PONG_MAX_BALLS] = { { true, 0.f, 0.f, 95.f, 25.f, 95.f }, { false, 0.f, 0.f, 0.f, 0.f, 95.f } };
global_variable float serve_dir = 1.f;

global_variable int score_1 = 0, score_2 = 0;
global_variable int current_rally = 0, best_rally = 0;
global_variable float serve_timer = 0.f;
global_variable float shake_timer = 0.f;
global_variable float shake_intensity = 0.f;

global_variable Powerup_Type active_powerup = PWR_NONE;
global_variable float powerup_x = 0.f, powerup_y = 0.f;
global_variable float powerup_spawn_timer = 5.f;
global_variable float powerup_pulse = 0.f;
global_variable u32   rng_state = 1337u;

#define PONG_TRAIL_LEN 8
global_variable Trail_Point trail[PONG_TRAIL_LEN];
global_variable int trail_head = 0;
global_variable float trail_timer = 0.f;

#define PONG_MAX_PARTICLES 48
global_variable Particle particles[PONG_MAX_PARTICLES];

internal u32
next_rand() {
	rng_state = rng_state * 1664525u + 1013904223u;
	return rng_state;
}

internal void
sfx(int freq_hz, int duration_ms, float volume = 0.22f, float freq_end_hz = 0.f) {
	if (!is_audio_muted()) play_tone(freq_hz, duration_ms, volume, freq_end_hz);
}

internal bool
in_rect(float px, float py, float cx, float cy, float hw, float hh) {
	return absf(px - cx) <= hw && absf(py - cy) <= hh;
}

internal void
clear_trail() {
	for (int i = 0; i < PONG_TRAIL_LEN; i++) {
		trail[i].x = balls[0].x;
		trail[i].y = balls[0].y;
	}
}

internal void
spawn_particles(float x, float y, u32 color, int count) {
	int spawned = 0;
	for (int i = 0; i < PONG_MAX_PARTICLES && spawned < count; i++) {
		if (particles[i].life <= 0.f) {
			float angle = (float)((spawned * 137) % 360) * 0.0174533f;
			float speed = 25.f + (float)((i * 19) % 45);
			particles[i].x = x;
			particles[i].y = y;
			particles[i].vx = cosf(angle) * speed;
			particles[i].vy = sinf(angle) * speed;
			particles[i].max_life = 0.45f;
			particles[i].life = 0.45f;
			particles[i].color = color;
			spawned++;
		}
	}
}

internal void
reset_balls(float dir_x) {
	serve_dir = dir_x;
	balls[0].active = true;
	balls[0].x = 0.f;
	balls[0].y = 0.f;
	balls[0].speed_base = 95.f;
	balls[0].dx = dir_x * balls[0].speed_base;
	balls[0].dy = ((score_1 + score_2) % 2 == 0 ? 18.f : -18.f);
	balls[1].active = false;
	current_rally = 0;
	serve_timer = 0.65f;
	clear_trail();
}

internal void
start_new_match() {
	score_1 = 0;
	score_2 = 0;
	best_rally = 0;
	player_1_p = 0.f; player_1_dp = 0.f;
	player_2_p = 0.f; player_2_dp = 0.f;
	p1_half_y = base_paddle_half_y;
	p2_half_y = base_paddle_half_y;
	p1_big_timer = 0.f;
	p2_big_timer = 0.f;
	p1_flash = 0.f;
	p2_flash = 0.f;
	active_powerup = PWR_NONE;
	powerup_spawn_timer = 5.5f;
	for (int i = 0; i < PONG_MAX_PARTICLES; i++) particles[i].life = 0.f;
	reset_balls(1.f);
	game_state = STATE_PLAYING;
	sfx(660, 80);
}

internal void
update_paddle_physics(float* p, float* dp, float half_y, float ddp, float dt) {
	ddp -= (*dp) * 10.f;
	*p = *p + (*dp) * dt + ddp * dt * dt * 0.5f;
	*dp = *dp + ddp * dt;

	if (*p + half_y > arena_half_size_y) {
		*p = arena_half_size_y - half_y;
		*dp *= -0.5f;
	} else if (*p - half_y < -arena_half_size_y) {
		*p = -arena_half_size_y + half_y;
		*dp *= -0.5f;
	}
}

internal u32
powerup_color(Powerup_Type p) {
	if (p == PWR_BIG_PADDLE) return 0x22C55E;
	if (p == PWR_MULTIBALL)  return 0xEC4899;
	if (p == PWR_SPEED)      return 0xEAB308;
	return 0xFFFFFF;
}

internal void
draw_arena_and_entities(const Theme& th, bool show_hud, float fps) {
	clear_screen(th.bg_outer);

	draw_rect(0, 0, arena_half_size_x + 1.2f, arena_half_size_y + 1.2f, th.arena_border);
	draw_rect(0, 0, arena_half_size_x, arena_half_size_y, th.arena_bg);

	for (float y = -arena_half_size_y + 3.f; y < arena_half_size_y - 2.f; y += 6.f) {
		draw_rect(0, y, 0.5f, 1.8f, th.center_line);
	}

	if (show_hud) {
		draw_number(score_1, -22.f, 32.f, 1.3f, th.p1);
		draw_number(score_2,  22.f, 32.f, 1.3f, th.p2);

		if (current_rally >= 3) {
			draw_text_centered("RALLY", -6.f, -40.f, 0.45f, th.text_dim);
			draw_number(current_rally, 10.f, -40.f, 0.45f, th.accent);
		}
		if (is_audio_muted()) {
			draw_text_centered("MUTED", 72.f, 40.f, 0.4f, 0xEF4444);
		}
		if (show_perf_hud) {
			draw_text_centered("FPS", -76.f, 40.f, 0.38f, th.text_dim);
			draw_number((int)(fps + 0.5f), -62.f, 40.f, 0.38f, th.accent);
			draw_text_centered("SPD", -76.f, 35.f, 0.38f, th.text_dim);
			draw_number((int)(balls[0].speed_base), -62.f, 35.f, 0.38f, th.text);
		}
	}

	if (show_hud && active_powerup != PWR_NONE) {
		float pulse = 2.6f + 0.4f * sinf(powerup_pulse * 6.f);
		u32 pcol = powerup_color(active_powerup);
		draw_rect_alpha(powerup_x, powerup_y, pulse + 1.f, pulse + 1.f, pcol, 0.25f);
		draw_rect(powerup_x, powerup_y, pulse, pulse, pcol);
		const char* tag = "B";
		if (active_powerup == PWR_MULTIBALL) tag = "M";
		else if (active_powerup == PWR_SPEED) tag = "S";
		draw_text_centered(tag, powerup_x, powerup_y, 0.42f, 0x050814);
	}

	for (int i = 0; i < PONG_TRAIL_LEN; i++) {
		int idx = (trail_head + i) % PONG_TRAIL_LEN;
		float alpha = ((float)(i + 1) / (float)PONG_TRAIL_LEN) * 0.28f;
		float size = ball_half_size * (0.4f + 0.5f * ((float)i / (float)PONG_TRAIL_LEN));
		draw_rect_alpha(trail[idx].x, trail[idx].y, size, size, th.accent, alpha);
	}

	for (int i = 0; i < PONG_MAX_PARTICLES; i++) {
		if (particles[i].life > 0.f) {
			float alpha = particles[i].life / particles[i].max_life;
			draw_rect_alpha(particles[i].x, particles[i].y, 0.7f, 0.7f, particles[i].color, alpha);
		}
	}

	u32 c1 = lerp_color(th.p1, 0xFFFFFF, p1_flash);
	u32 c2 = lerp_color(th.p2, 0xFFFFFF, p2_flash);
	draw_rect(-80.f, player_1_p, player_half_size_x, p1_half_y, c1);
	draw_rect( 80.f, player_2_p, player_half_size_x, p2_half_y, c2);

	for (int b = 0; b < PONG_MAX_BALLS; b++) {
		if (balls[b].active) {
			u32 bcol = (b == 0) ? th.ball : th.accent;
			draw_rect(balls[b].x, balls[b].y, ball_half_size, ball_half_size, bcol);
		}
	}

	if (show_hud && serve_timer > 0.f) {
		const char* arrow = serve_dir > 0.f ? ">" : "<";
		draw_text_centered(arrow, serve_dir * 7.f, 0.f, 0.65f, th.accent);
	}
}

void
init() {
	game_state = STATE_INTRO;
	intro_timer = 0.f;
	intro_stage = 0;
	menu_selection = 0;
	pause_selection = 0;
	gameover_selection = 0;
	p1_half_y = base_paddle_half_y;
	p2_half_y = base_paddle_half_y;
	p1_big_timer = 0.f;
	p2_big_timer = 0.f;
	p1_flash = 0.f;
	p2_flash = 0.f;
	active_powerup = PWR_NONE;
	shake_timer = 0.f;
	render_offset_x = 0.f;
	render_offset_y = 0.f;
	for (int i = 0; i < PONG_MAX_PARTICLES; i++) particles[i].life = 0.f;
	reset_balls(1.f);
}

Game_Result
update(Input* input, float dt, float fps) {
	const Theme& th = resolve_theme(user_stats.pong_theme);

	// Hide OS cursor during active gameplay and intro splash
	set_cursor_visible(game_state != STATE_PLAYING && game_state != STATE_INTRO);

	if (game_state == STATE_CONTROLS) {
		int tab = 0;
		if (update_and_draw_keybinds_ui(input, &tab, false,
		                                th.bg_outer, th.arena_bg, th.arena_border,
		                                th.center_line, th.text, th.text_dim, th.accent)) {
			save_user_data();
			game_state = controls_return_state;
		}
		return GAME_CONTINUE;
	}

	if (game_state == STATE_SOUND) {
		if (update_and_draw_audio_ui(input,
		                             th.bg_outer, th.arena_bg, th.arena_border,
		                             th.center_line, th.text, th.text_dim, th.accent)) {
			save_user_data();
			game_state = controls_return_state;
		}
		return GAME_CONTINUE;
	}

	if (game_state == STATE_GRAPHICS) {
		if (update_and_draw_graphics_ui(input,
		                                th.bg_outer, th.arena_bg, th.arena_border,
		                                th.center_line, th.text, th.text_dim, th.accent)) {
			save_user_data();
			game_state = controls_return_state;
		}
		return GAME_CONTINUE;
	}

	if (pressed(BUTTON_M)) {
		set_audio_muted(!is_audio_muted());
		if (!is_audio_muted()) sfx(580, 40, 0.15f);
		save_user_data();
	}
	if (pressed(BUTTON_F1)) {
		show_perf_hud = !show_perf_hud;
	}

	if (shake_timer > 0.f) {
		shake_timer -= dt;
		if (shake_timer <= 0.f) {
			render_offset_x = 0.f;
			render_offset_y = 0.f;
		} else {
			float phase = shake_timer * 90.f;
			render_offset_x = sinf(phase) * shake_intensity * (shake_timer / 0.18f);
			render_offset_y = cosf(phase * 1.3f) * shake_intensity * (shake_timer / 0.18f);
		}
	}

	if (p1_flash > 0.f) p1_flash = clampf(0.f, p1_flash - dt * 6.f, 1.f);
	if (p2_flash > 0.f) p2_flash = clampf(0.f, p2_flash - dt * 6.f, 1.f);

	for (int i = 0; i < PONG_MAX_PARTICLES; i++) {
		if (particles[i].life > 0.f) {
			particles[i].x += particles[i].vx * dt;
			particles[i].y += particles[i].vy * dt;
			particles[i].life -= dt;
		}
	}

	// -----------------------------------------------------------------------
	// CREATIVE INTRO SPLASH (STATE_INTRO)
	// -----------------------------------------------------------------------
	if (game_state == STATE_INTRO) {
		intro_timer += dt;
		if (intro_stage == 0) {
			intro_stage = 1;
			sfx(320, 140, 0.20f, 640.f);
		} else if (intro_stage == 1 && intro_timer >= 0.7f) {
			intro_stage = 2;
			sfx(523, 180, 0.24f, 880.f);
			spawn_particles(0.f, 0.f, th.accent, 28);
		}

		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE) ||
		    pressed(BUTTON_ESC) || pressed(BUTTON_MOUSE_LEFT) || intro_timer >= 2.0f) {
			game_state = STATE_MENU;
			return GAME_CONTINUE;
		}

		clear_screen(th.bg_outer);
		draw_rect(0, 0, arena_half_size_x + 1.2f, arena_half_size_y + 1.2f, th.arena_border);
		draw_rect(0, 0, arena_half_size_x, arena_half_size_y, th.arena_bg);

		float t = clampf(0.f, intro_timer / 0.7f, 1.f);
		float px = 80.f + (1.f - t) * 18.f;
		draw_rect(-px, sinf(intro_timer * 5.f) * 14.f, player_half_size_x, base_paddle_half_y, th.p1);
		draw_rect( px, -sinf(intro_timer * 5.f) * 14.f, player_half_size_x, base_paddle_half_y, th.p2);

		float bx = sinf(intro_timer * 6.5f) * 65.f;
		float by = cosf(intro_timer * 4.5f) * 24.f;
		draw_rect(bx, by, 1.5f, 1.5f, th.ball);

		for (int i = 0; i < PONG_MAX_PARTICLES; i++) {
			if (particles[i].life > 0.f) {
				float alpha = particles[i].life / particles[i].max_life;
				draw_rect_alpha(particles[i].x, particles[i].y, 0.75f, 0.75f, particles[i].color, alpha);
			}
		}

		if (intro_timer >= 0.7f) {
			draw_text_centered("PONG ARENA", 0.f, 6.f, 1.35f, th.accent);
			draw_text_centered("STARTING GAME...", 0.f, -12.f, 0.42f, th.text);
		} else {
			draw_text_centered("LOADING ARENA...", 0.f, 0.f, 0.45f, th.text_dim);
		}
		return GAME_CONTINUE;
	}

	// -----------------------------------------------------------------------
	// MAIN MENU (Keyboard + Mouse)
	// -----------------------------------------------------------------------
	if (game_state == STATE_MENU) {
		if (pressed(BUTTON_ESC)) {
			render_offset_x = 0.f; render_offset_y = 0.f;
			return GAME_EXIT_TO_HUB;
		}
		balls[0].active = true;
		balls[1].active = false;
		balls[0].x += balls[0].dx * dt * 0.6f;
		balls[0].y += balls[0].dy * dt * 0.6f;
		if (balls[0].x > arena_half_size_x - ball_half_size || balls[0].x < -arena_half_size_x + ball_half_size) balls[0].dx *= -1.f;
		if (balls[0].y > arena_half_size_y - ball_half_size || balls[0].y < -arena_half_size_y + ball_half_size) balls[0].dy *= -1.f;

		trail_timer += dt;
		if (trail_timer >= 0.02f) {
			trail_timer = 0.f;
			trail[trail_head].x = balls[0].x;
			trail[trail_head].y = balls[0].y;
			trail_head = (trail_head + 1) % PONG_TRAIL_LEN;
		}

		const int MENU_ITEMS = 8;
		if (pressed(BUTTON_UP) || pressed(BUTTON_W)) {
			menu_selection = (menu_selection + MENU_ITEMS - 1) % MENU_ITEMS;
			sfx(440, 35, 0.15f);
		}
		if (pressed(BUTTON_DOWN) || pressed(BUTTON_S)) {
			menu_selection = (menu_selection + 1) % MENU_ITEMS;
			sfx(440, 35, 0.15f);
		}

		float mx = input->mouse_x;
		float my = input->mouse_y;
		bool clicked = pressed(BUTTON_MOUSE_LEFT);
		for (int i = 0; i < MENU_ITEMS; i++) {
			float y = 16.f - i * 6.5f;
			if (in_rect(mx, my, 0.f, y, 48.f, 2.8f)) {
				if (input->mouse_moved && menu_selection != i) {
					menu_selection = i;
					sfx(440, 24, 0.10f);
				}
			}
		}

		int dir_change = 0;
		if (pressed(BUTTON_LEFT))  dir_change = -1;
		if (pressed(BUTTON_RIGHT)) dir_change = 1;
		bool activate = pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE) ||
		                (clicked && in_rect(mx, my, 0.f, 16.f - menu_selection * 6.5f, 48.f, 2.8f));

		if (activate) {
			if (menu_selection == 0) { is_single_player = true; start_new_match(); return GAME_CONTINUE; }
			else if (menu_selection == 1) { is_single_player = false; start_new_match(); return GAME_CONTINUE; }
			else if (menu_selection == 6) { controls_return_state = STATE_MENU; game_state = STATE_GRAPHICS; return GAME_CONTINUE; }
			else if (menu_selection == 7) {
				render_offset_x = 0.f; render_offset_y = 0.f;
				return GAME_EXIT_TO_HUB;
			}
			else dir_change = 1;
		}
		if (dir_change != 0) {
			if (menu_selection == 2) difficulty = (Difficulty)(((int)difficulty + (dir_change > 0 ? 1 : 2)) % 3);
			else if (menu_selection == 3) arcade_mode = !arcade_mode;
			else if (menu_selection == 4) win_score_idx = (win_score_idx + (dir_change > 0 ? 1 : 2)) % 3;
			else if (menu_selection == 5) {
				// Cycle -1 (FOLLOW HUB) -> 0 -> 1 -> 2
				int cur = user_stats.pong_theme + 1; // 0..3
				cur = (cur + (dir_change > 0 ? 1 : 3)) % 4;
				user_stats.pong_theme = cur - 1;
				save_user_data();
			}
			sfx(520, 35, 0.15f);
		}

		draw_arena_and_entities(th, false, fps);
		draw_rect_alpha(0, 0, arena_half_size_x, arena_half_size_y, 0x050814, 0.70f);

		draw_text_centered("PONG ARENA", 0.f, 32.f, 1.25f, th.accent);

		const char* diff_label = (difficulty == DIFF_EASY) ? "DIFFICULTY: EASY" :
		                         (difficulty == DIFF_HARD) ? "DIFFICULTY: HARD" : "DIFFICULTY: MEDIUM";
		const char* arc_label  = arcade_mode ? "ARCADE POWERUPS: ON" : "ARCADE POWERUPS: OFF";
		const char* win_label  = (win_score_idx == 0) ? "WIN SCORE: 5" :
		                         (win_score_idx == 2) ? "WIN SCORE: 11" : "WIN SCORE: 7";
		const char* thm_label  = (user_stats.pong_theme < 0) ? "THEME: FOLLOW HUB" :
		                         (user_stats.pong_theme == 1) ? "THEME: CRT" :
		                         (user_stats.pong_theme == 2) ? "THEME: SYNTHWAVE" : "THEME: CYBER";

		const char* labels[8] = {
			"1 PLAYER (VS AI)",
			"2 PLAYERS (LOCAL)",
			diff_label,
			arc_label,
			win_label,
			thm_label,
			"GRAPHICS",
			"BACK TO HUB"
		};

		for (int i = 0; i < MENU_ITEMS; i++) {
			float y = 16.f - i * 6.5f;
			u32 col = th.text;
			if (i == menu_selection) col = (i == 7) ? 0xEF4444 : ((i >= 2 && i <= 5) ? th.p2 : th.p1);
			draw_text_centered(labels[i], 0.f, y, 0.50f, col);
		}

		float sel_y = 16.f - menu_selection * 6.5f;
		draw_text_centered(">", -48.f, sel_y, 0.50f, th.accent);
		draw_text_centered("<",  48.f, sel_y, 0.50f, th.accent);

		draw_text_centered("CLICK/ENTER: SELECT   ESC: HUB   M: MUTE   F1: PERF HUD", 0.f, -40.f, 0.34f, th.text_dim);
		return GAME_CONTINUE;
	}

	if (game_state == STATE_PAUSED) {
		const int PAUSE_ITEMS = 6;
		if (vk_pressed(input, pong_binds.pause) || pressed(BUTTON_P)) {
			game_state = STATE_PLAYING;
			sfx(520, 45, 0.15f);
			return GAME_CONTINUE;
		}
		if (pressed(BUTTON_UP) || pressed(BUTTON_W)) {
			pause_selection = (pause_selection + PAUSE_ITEMS - 1) % PAUSE_ITEMS;
			sfx(440, 35, 0.15f);
		}
		if (pressed(BUTTON_DOWN) || pressed(BUTTON_S)) {
			pause_selection = (pause_selection + 1) % PAUSE_ITEMS;
			sfx(440, 35, 0.15f);
		}

		float mx = input->mouse_x;
		float my = input->mouse_y;
		bool activate = pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE);
		for (int i = 0; i < PAUSE_ITEMS; i++) {
			float y = 12.f - i * 7.2f;
			if (in_rect(mx, my, 0.f, y, 36.f, 3.0f)) {
				if (input->mouse_moved && pause_selection != i) {
					pause_selection = i;
					sfx(440, 24, 0.10f);
				}
				if (pressed(BUTTON_MOUSE_LEFT)) { pause_selection = i; activate = true; }
			}
		}

		if (activate) {
			if (pause_selection == 0) game_state = STATE_PLAYING;
			else if (pause_selection == 1) start_new_match();
			else if (pause_selection == 2) { controls_return_state = STATE_PAUSED; game_state = STATE_CONTROLS; }
			else if (pause_selection == 3) { controls_return_state = STATE_PAUSED; game_state = STATE_SOUND; }
			else if (pause_selection == 4) { controls_return_state = STATE_PAUSED; game_state = STATE_GRAPHICS; }
			else { game_state = STATE_MENU; pause_selection = 0; }
			sfx(600, 60, 0.2f);
		}

		draw_arena_and_entities(th, true, fps);
		draw_rect_alpha(0, 0, arena_half_size_x, arena_half_size_y, 0x050814, 0.74f);
		draw_text_centered("PAUSED", 0.f, 26.f, 1.0f, th.accent);
		const char* plabels[6] = { "RESUME", "RESTART", "CONTROLS", "SOUND", "GRAPHICS", "MAIN MENU" };
		for (int i = 0; i < PAUSE_ITEMS; i++) {
			float y = 12.f - i * 7.2f;
			u32 col = (i == pause_selection) ? (i == 5 ? th.p2 : th.p1) : th.text;
			draw_text_centered(plabels[i], 0.f, y, 0.52f, col);
		}
		draw_text_centered(">", -28.f, 12.f - pause_selection * 7.2f, 0.52f, th.accent);
		return GAME_CONTINUE;
	}

	if (game_state == STATE_GAME_OVER) {
		if (pressed(BUTTON_UP) || pressed(BUTTON_W) || pressed(BUTTON_DOWN) || pressed(BUTTON_S)) {
			gameover_selection = 1 - gameover_selection;
			sfx(440, 35, 0.15f);
		}
		float mx = input->mouse_x;
		float my = input->mouse_y;
		bool activate = pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE);
		if (in_rect(mx, my, 0.f, -8.f, 34.f, 3.5f)) {
			if (input->mouse_moved) gameover_selection = 0;
			if (pressed(BUTTON_MOUSE_LEFT)) { gameover_selection = 0; activate = true; }
		}
		if (in_rect(mx, my, 0.f, -17.f, 34.f, 3.5f)) {
			if (input->mouse_moved) gameover_selection = 1;
			if (pressed(BUTTON_MOUSE_LEFT)) { gameover_selection = 1; activate = true; }
		}
		if (activate) {
			if (gameover_selection == 0) start_new_match();
			else game_state = STATE_MENU;
		}

		draw_arena_and_entities(th, true, fps);
		draw_rect_alpha(0, 0, arena_half_size_x, arena_half_size_y, 0x050814, 0.78f);

		int target = WIN_SCORES[win_score_idx];
		const char* winner = "PLAYER 1 WINS!";
		u32 win_col = th.p1;
		if (score_2 >= target) {
			winner = is_single_player ? "AI WINS!" : "PLAYER 2 WINS!";
			win_col = th.p2;
		}
		draw_text_centered(winner, 0.f, 20.f, 1.0f, win_col);
		draw_text_centered("BEST RALLY:", -8.f, 6.f, 0.55f, th.text_dim);
		draw_number(best_rally, 20.f, 6.f, 0.55f, th.accent);

		draw_text_centered("PLAY AGAIN", 0.f, -8.f, 0.65f, gameover_selection == 0 ? th.p1 : th.text);
		draw_text_centered("MAIN MENU",  0.f, -17.f, 0.65f, gameover_selection == 1 ? th.p2 : th.text);
		draw_text_centered(">", -28.f, gameover_selection == 0 ? -8.f : -17.f, 0.65f, th.accent);
		return GAME_CONTINUE;
	}

	// STATE_PLAYING
	if (vk_pressed(input, pong_binds.pause) || pressed(BUTTON_P)) {
		pause_selection = 0;
		game_state = STATE_PAUSED;
		sfx(380, 45, 0.15f);
		return GAME_CONTINUE;
	}

	if (p1_big_timer > 0.f) {
		p1_big_timer -= dt;
		if (p1_big_timer <= 0.f) p1_half_y = base_paddle_half_y;
	}
	if (p2_big_timer > 0.f) {
		p2_big_timer -= dt;
		if (p2_big_timer <= 0.f) p2_half_y = base_paddle_half_y;
	}

	if (arcade_mode && serve_timer <= 0.f) {
		powerup_pulse += dt;
		if (active_powerup == PWR_NONE) {
			powerup_spawn_timer -= dt;
			if (powerup_spawn_timer <= 0.f) {
				active_powerup = (Powerup_Type)(1 + ((next_rand() >> 16) % 3));
				powerup_x = (float)((s32)((next_rand() >> 16) % 41) - 20);
				powerup_y = (float)((s32)((next_rand() >> 16) % 57) - 28);
				powerup_spawn_timer = 8.f;
			}
		}
	}

	float player_1_ddp = 0.f;
	if (vk_down(input, pong_binds.p1_up)   || (is_single_player && vk_down(input, pong_binds.p2_up)))   player_1_ddp += 2200.f;
	if (vk_down(input, pong_binds.p1_down) || (is_single_player && vk_down(input, pong_binds.p2_down))) player_1_ddp -= 2200.f;

	float player_2_ddp = 0.f;
	if (!is_single_player) {
		if (vk_down(input, pong_binds.p2_up))   player_2_ddp += 2200.f;
		if (vk_down(input, pong_binds.p2_down)) player_2_ddp -= 2200.f;
	} else {
		if (pong_binds.mouse_1p && input->mouse_moved) {
			player_1_p = clampf(-arena_half_size_y + p1_half_y, input->mouse_y, arena_half_size_y - p1_half_y);
			player_1_dp = 0.f;
			player_1_ddp = 0.f;
		}
		float ai_accel = 1300.f;
		float deadzone = 3.5f;
		if (difficulty == DIFF_MEDIUM) { ai_accel = 1750.f; deadzone = 2.0f; }
		else if (difficulty == DIFF_HARD) { ai_accel = 2300.f; deadzone = 0.8f; }

		float target_y = 0.f;
		float best_x = -1000.f;
		for (int b = 0; b < PONG_MAX_BALLS; b++) {
			if (balls[b].active && balls[b].dx > 0.f && balls[b].x > best_x) {
				best_x = balls[b].x;
				target_y = balls[b].y;
			}
		}
		float diff = target_y - player_2_p;
		if (diff > deadzone) player_2_ddp = ai_accel;
		else if (diff < -deadzone) player_2_ddp = -ai_accel;
	}

	update_paddle_physics(&player_1_p, &player_1_dp, p1_half_y, player_1_ddp, dt);
	update_paddle_physics(&player_2_p, &player_2_dp, p2_half_y, player_2_ddp, dt);

	if (serve_timer > 0.f) {
		serve_timer -= dt;
	} else {
		trail_timer += dt;
		if (trail_timer >= 0.015f) {
			trail_timer = 0.f;
			trail[trail_head].x = balls[0].x;
			trail[trail_head].y = balls[0].y;
			trail_head = (trail_head + 1) % PONG_TRAIL_LEN;
		}

		int target_score = WIN_SCORES[win_score_idx];

		for (int b = 0; b < PONG_MAX_BALLS; b++) {
			if (!balls[b].active) continue;

			balls[b].x += balls[b].dx * dt;
			balls[b].y += balls[b].dy * dt;

			if (active_powerup != PWR_NONE &&
				absf(balls[b].x - powerup_x) < (ball_half_size + 3.2f) &&
				absf(balls[b].y - powerup_y) < (ball_half_size + 3.2f)) {
				spawn_particles(powerup_x, powerup_y, powerup_color(active_powerup), 16);
				sfx(820, 70, 0.22f);
				if (active_powerup == PWR_BIG_PADDLE) {
					if (last_hitter == 1) { p1_half_y = 16.5f; p1_big_timer = 8.f; }
					else                  { p2_half_y = 16.5f; p2_big_timer = 8.f; }
				} else if (active_powerup == PWR_MULTIBALL && !balls[1].active) {
					balls[1].active = true;
					balls[1].x = balls[b].x;
					balls[1].y = balls[b].y;
					balls[1].speed_base = balls[b].speed_base;
					balls[1].dx = -balls[b].dx;
					balls[1].dy = -balls[b].dy;
				} else if (active_powerup == PWR_SPEED) {
					balls[b].speed_base = clampf(95.f, balls[b].speed_base * 1.25f, 220.f);
					balls[b].dx = (balls[b].dx > 0.f ? 1.f : -1.f) * balls[b].speed_base;
				}
				active_powerup = PWR_NONE;
			}

			if (balls[b].dx > 0.f &&
				balls[b].x + ball_half_size > 80.f - player_half_size_x &&
				balls[b].x - ball_half_size < 80.f + player_half_size_x &&
				balls[b].y + ball_half_size > player_2_p - p2_half_y &&
				balls[b].y - ball_half_size < player_2_p + p2_half_y) {
				balls[b].x = 80.f - player_half_size_x - ball_half_size;
				balls[b].speed_base = clampf(95.f, balls[b].speed_base * 1.05f, 220.f);
				balls[b].dx = -balls[b].speed_base;
				balls[b].dy = (balls[b].y - player_2_p) * 3.2f + player_2_dp * 0.65f;
				p2_flash = 1.f;
				last_hitter = 2;
				current_rally++;
				if (current_rally > best_rally) best_rally = current_rally;
				shake_timer = 0.10f; shake_intensity = 0.45f;
				spawn_particles(balls[b].x, balls[b].y, th.p2, 8);
				sfx(540, 45, 0.22f);
			}
			else if (balls[b].dx < 0.f &&
				balls[b].x - ball_half_size < -80.f + player_half_size_x &&
				balls[b].x + ball_half_size > -80.f - player_half_size_x &&
				balls[b].y + ball_half_size > player_1_p - p1_half_y &&
				balls[b].y - ball_half_size < player_1_p + p1_half_y) {
				balls[b].x = -80.f + player_half_size_x + ball_half_size;
				balls[b].speed_base = clampf(95.f, balls[b].speed_base * 1.05f, 220.f);
				balls[b].dx = balls[b].speed_base;
				balls[b].dy = (balls[b].y - player_1_p) * 3.2f + player_1_dp * 0.65f;
				p1_flash = 1.f;
				last_hitter = 1;
				current_rally++;
				if (current_rally > best_rally) best_rally = current_rally;
				shake_timer = 0.10f; shake_intensity = 0.45f;
				spawn_particles(balls[b].x, balls[b].y, th.p1, 8);
				sfx(480, 45, 0.22f);
			}

			if (balls[b].y + ball_half_size > arena_half_size_y) {
				balls[b].y = arena_half_size_y - ball_half_size;
				balls[b].dy *= -1.f;
				sfx(320, 30, 0.14f);
			} else if (balls[b].y - ball_half_size < -arena_half_size_y) {
				balls[b].y = -arena_half_size_y + ball_half_size;
				balls[b].dy *= -1.f;
				sfx(320, 30, 0.14f);
			}

			if (balls[b].x + ball_half_size > arena_half_size_x) {
				score_1++;
				spawn_particles(balls[b].x, balls[b].y, th.p1, 20);
				shake_timer = 0.18f; shake_intensity = 1.1f;
				sfx(220, 140, 0.25f);
				if (score_1 >= target_score) {
					gameover_selection = 0;
					game_state = STATE_GAME_OVER;
					if (best_rally > user_stats.pong_best_rally) user_stats.pong_best_rally = best_rally;
					user_stats.pong_wins++;
					save_user_data();
					sfx(780, 220, 0.25f);
					break;
				} else {
					reset_balls(-1.f);
					break;
				}
			} else if (balls[b].x - ball_half_size < -arena_half_size_x) {
				score_2++;
				spawn_particles(balls[b].x, balls[b].y, th.p2, 20);
				shake_timer = 0.18f; shake_intensity = 1.1f;
				sfx(220, 140, 0.25f);
				if (score_2 >= target_score) {
					gameover_selection = 0;
					game_state = STATE_GAME_OVER;
					if (best_rally > user_stats.pong_best_rally) user_stats.pong_best_rally = best_rally;
					save_user_data();
					sfx(780, 220, 0.25f);
					break;
				} else {
					reset_balls(1.f);
					break;
				}
			}
		}
	}

	draw_arena_and_entities(th, true, fps);
	return GAME_CONTINUE;
}

} // namespace Pong

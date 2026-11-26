#include <math.h>
#include "utils.h"
#include "renderer.h"
#include "keybinds.h"
#include "audio_ui.h"
#include "game_interface.h"

#define is_down(b) input->buttons[b].is_down
#define pressed(b) (input->buttons[b].is_down && input->buttons[b].changed)

namespace Breaker {

enum Game_State {
	STATE_MENU,
	STATE_LEVEL_SELECT,
	STATE_CONTROLS,
	STATE_SOUND,
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_LEVEL_CLEAR,
	STATE_GAME_OVER,
	STATE_VICTORY
};

enum Brick_Kind {
	BRICK_NONE = 0,
	BRICK_1HIT = 1,
	BRICK_2HIT = 2,
	BRICK_3HIT = 3,
	BRICK_STEEL = 4,
	BRICK_TNT = 5
};

enum Powerup_Kind {
	PWR_WIDE = 0,
	PWR_MULTIBALL = 1,
	PWR_LASER = 2,
	PWR_LIFE = 3
};

struct Brick {
	Brick_Kind kind;
	int hp;
	float x, y;
	float flash;
};

struct Ball {
	bool active;
	bool stuck;
	float x, y;
	float dx, dy;
	float speed;
};

struct Powerup {
	bool active;
	Powerup_Kind kind;
	float x, y;
};

struct Laser {
	bool active;
	float x, y;
};

struct Particle {
	float x, y, vx, vy, life, max_life;
	u32 color;
};

#define BRK_NUM_LEVELS    8
#define BRK_COLS          10
#define BRK_ROWS          6
#define BRK_MAX_BALLS     3
#define BRK_MAX_POWERUPS  4
#define BRK_MAX_LASERS    8
#define BRK_MAX_PARTICLES 64

global_variable const float arena_half_x = 85.f;
global_variable const float arena_half_y = 45.f;
global_variable const float paddle_y = -38.f;
global_variable const float base_paddle_hx = 11.f;
global_variable const float paddle_hy = 1.6f;
global_variable const float ball_r = 1.25f;
global_variable const float brick_hx = 7.2f;
global_variable const float brick_hy = 2.3f;

global_variable const char* LEVEL_NAMES[BRK_NUM_LEVELS] = {
	"1. NEON GATE",
	"2. ARMOR WALL",
	"3. CHAIN REACTION",
	"4. STEEL FORTRESS",
	"5. DIAMOND CORE",
	"6. TWIN TOWERS",
	"7. GAUNTLET",
	"8. BOSS CORE"
};

global_variable const char* LEVELS[BRK_NUM_LEVELS][BRK_ROWS] = {
	{ "..........", ".11111111.", ".11111111.", ".11111111.", "..........", ".........." },
	{ "1111111111", "1111111111", "2222222222", "..........", "..........", ".........." },
	{ "2222222222", "21B1221B12", "2111B11112", "2222222222", "..........", ".........." },
	{ "1111111111", "22.2222.22", "XX.2B22.XX", "11.1111.11", "..........", ".........." },
	{ "....33....", "...3223...", "..32BB23..", "...3223...", "....33....", ".........." },
	{ "333....333", "2B2....2B2", "222.11.222", "XX2.22.2XX", "111....111", ".........." },
	{ "3232323232", "2B2X22X2B2", "3232B23232", "2X222222X2", "1111111111", ".........." },
	{ "X33333333X", "X3B3333B3X", "X333BB333X", "XX222222XX", "..111111..", ".........." }
};

global_variable Game_State game_state = STATE_MENU;
global_variable int menu_sel = 0;
global_variable int level_sel = 0;
global_variable int pause_sel = 0;
global_variable Game_State controls_return = STATE_MENU;
global_variable int current_level = 0;

global_variable int score = 0;
global_variable int lives = 3;
global_variable int combo = 0;

global_variable float paddle_x = 0.f;
global_variable float paddle_dx = 0.f;
global_variable float paddle_hx = base_paddle_hx;
global_variable float wide_timer = 0.f;
global_variable float laser_timer = 0.f;
global_variable float laser_cd = 0.f;

global_variable Brick bricks[BRK_ROWS][BRK_COLS];
global_variable Ball balls[BRK_MAX_BALLS];
global_variable Powerup powerups[BRK_MAX_POWERUPS];
global_variable Laser lasers[BRK_MAX_LASERS];
global_variable Particle particles[BRK_MAX_PARTICLES];

global_variable float shake_timer = 0.f;
global_variable float shake_mag = 0.f;
global_variable u32 rng_state = 7777u;

internal u32
next_rand() {
	rng_state = rng_state * 1664525u + 1013904223u;
	return rng_state;
}

internal void
sfx(int freq_hz, int duration_ms, float vol = 0.22f, int freq_end_hz = 0) {
	if (!is_audio_muted()) play_tone(freq_hz, duration_ms, vol, (float)freq_end_hz);
}

internal u32
brick_color(const Brick* b) {
	u32 base = 0x38BDF8;
	if (b->kind == BRICK_1HIT) base = 0x38BDF8;
	else if (b->kind == BRICK_2HIT) base = (b->hp == 2) ? 0xA855F7 : 0x38BDF8;
	else if (b->kind == BRICK_3HIT) base = (b->hp == 3) ? 0xF43F5E : (b->hp == 2 ? 0xF59E0B : 0x38BDF8);
	else if (b->kind == BRICK_STEEL) base = 0x64748B;
	else if (b->kind == BRICK_TNT) base = 0xEF4444;
	return lerp_color(base, 0xFFFFFF, b->flash);
}

internal void
spawn_particles(float x, float y, u32 color, int count) {
	int spawned = 0;
	for (int i = 0; i < BRK_MAX_PARTICLES && spawned < count; i++) {
		if (particles[i].life <= 0.f) {
			float angle = (float)((spawned * 137 + i * 31) % 360) * 0.0174533f;
			float speed = 20.f + (float)((i * 19) % 45);
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
reset_ball_on_paddle() {
	for (int i = 0; i < BRK_MAX_BALLS; i++) balls[i].active = false;
	balls[0].active = true;
	balls[0].stuck = true;
	balls[0].x = paddle_x;
	balls[0].y = paddle_y + paddle_hy + ball_r + 0.1f;
	balls[0].speed = 76.f + (float)current_level * 3.5f;
	balls[0].dx = balls[0].speed * 0.45f;
	balls[0].dy = balls[0].speed * 0.89f;
	combo = 0;
}

internal void
load_level(int lvl) {
	current_level = clamp(0, lvl, BRK_NUM_LEVELS - 1);

	paddle_x = 0.f;
	paddle_dx = 0.f;
	paddle_hx = base_paddle_hx;
	wide_timer = 0.f;
	laser_timer = 0.f;
	laser_cd = 0.f;

	for (int i = 0; i < BRK_MAX_POWERUPS; i++) powerups[i].active = false;
	for (int i = 0; i < BRK_MAX_LASERS; i++) lasers[i].active = false;

	float start_x = -67.5f;
	float step_x = 15.0f;
	float start_y = 28.0f;
	float step_y = 5.4f;

	for (int r = 0; r < BRK_ROWS; r++) {
		for (int c = 0; c < BRK_COLS; c++) {
			Brick* b = &bricks[r][c];
			b->x = start_x + c * step_x;
			b->y = start_y - r * step_y;
			b->flash = 0.f;
			char ch = LEVELS[current_level][r][c];
			if (ch == '1')      { b->kind = BRICK_1HIT;  b->hp = 1; }
			else if (ch == '2') { b->kind = BRICK_2HIT;  b->hp = 2; }
			else if (ch == '3') { b->kind = BRICK_3HIT;  b->hp = 3; }
			else if (ch == 'X') { b->kind = BRICK_STEEL; b->hp = 99; }
			else if (ch == 'B') { b->kind = BRICK_TNT;   b->hp = 1; }
			else                { b->kind = BRICK_NONE;  b->hp = 0; }
		}
	}

	reset_ball_on_paddle();
	game_state = STATE_PLAYING;
	sfx(620, 90);
}

internal void
start_campaign(int start_lvl) {
	score = 0;
	lives = 3;
	for (int i = 0; i < BRK_MAX_PARTICLES; i++) particles[i].life = 0.f;
	load_level(start_lvl);
}

internal void
maybe_spawn_powerup(float x, float y) {
	if (((next_rand() >> 16) % 100) >= 28) return;
	for (int i = 0; i < BRK_MAX_POWERUPS; i++) {
		if (!powerups[i].active) {
			powerups[i].active = true;
			powerups[i].x = x;
			powerups[i].y = y;
			powerups[i].kind = (Powerup_Kind)((next_rand() >> 16) % 4);
			break;
		}
	}
}

internal void
damage_brick(int r, int c) {
	if (r < 0 || r >= BRK_ROWS || c < 0 || c >= BRK_COLS) return;
	Brick* b = &bricks[r][c];
	if (b->kind == BRICK_NONE) return;
	if (b->kind == BRICK_STEEL) {
		b->flash = 1.f;
		sfx(960, 40, 0.16f, 420);
		return;
	}

	b->hp--;
	b->flash = 1.f;
	if (b->hp <= 0) {
		Brick_Kind destroyed_kind = b->kind;
		b->kind = BRICK_NONE;
		combo++;
		score += 50 * combo;
		spawn_particles(b->x, b->y, (destroyed_kind == BRICK_TNT) ? 0xFACC15 : 0x38BDF8, 10);

		if (destroyed_kind == BRICK_TNT) {
			shake_timer = 0.20f;
			shake_mag = 1.1f;
			sfx(240, 220, 0.32f, 55);
			for (int dr = -1; dr <= 1; dr++) {
				for (int dc = -1; dc <= 1; dc++) {
					if (dr != 0 || dc != 0) damage_brick(r + dr, c + dc);
				}
			}
		} else {
			int pitch_boost = clamp(0, combo * 35, 350);
			if (destroyed_kind == BRICK_3HIT) {
				sfx(700 + pitch_boost, 60, 0.22f, 1040 + pitch_boost);
			} else if (destroyed_kind == BRICK_2HIT) {
				sfx(600 + pitch_boost, 50, 0.20f, 880 + pitch_boost);
			} else {
				sfx(520 + pitch_boost, 45, 0.18f, 760 + pitch_boost);
			}
			maybe_spawn_powerup(b->x, b->y);
		}
	} else {
		if (b->kind == BRICK_3HIT && b->hp == 2) {
			sfx(210, 60, 0.19f, 130);
		} else if (b->kind == BRICK_3HIT) {
			sfx(340, 50, 0.18f, 220);
		} else {
			sfx(300, 50, 0.18f, 180);
		}
	}
}

internal int
count_breakable_bricks() {
	int rem = 0;
	for (int r = 0; r < BRK_ROWS; r++) {
		for (int c = 0; c < BRK_COLS; c++) {
			if (bricks[r][c].kind != BRICK_NONE && bricks[r][c].kind != BRICK_STEEL) rem++;
		}
	}
	return rem;
}

internal void
draw_world(bool show_hud) {
	clear_screen(0x0B1120);
	draw_rect(0.f, 0.f, arena_half_x + 1.2f, arena_half_y + 1.2f, 0x334155);
	draw_rect(0.f, 0.f, arena_half_x, arena_half_y, 0x0F172A);

	if (show_hud) {
		draw_text_centered("LVL", -74.f, 40.5f, 0.36f, 0x64748B);
		draw_number(current_level + 1, -62.f, 40.5f, 0.38f, 0x22D3EE);

		draw_text_centered("SCORE", -16.f, 40.5f, 0.36f, 0x64748B);
		draw_number(score, 8.f, 40.5f, 0.38f, 0xF8FAFC);

		draw_text_centered("LIVES", 56.f, 40.5f, 0.36f, 0x64748B);
		draw_number(lives, 72.f, 40.5f, 0.38f, 0x22C55E);

		if (combo >= 2) {
			draw_text_centered("X", 28.f, 40.5f, 0.36f, 0xFACC15);
			draw_number(combo, 35.f, 40.5f, 0.36f, 0xFACC15);
		}
		if (is_audio_muted()) {
			draw_text_centered("MUTED", 0.f, -42.5f, 0.32f, 0xEF4444);
		}
		if (wide_timer > 0.f) {
			draw_text_centered("WIDE", -72.f, -42.5f, 0.32f, 0x22C55E);
			draw_number((int)(wide_timer + 0.9f), -56.f, -42.5f, 0.32f, 0x22C55E);
		}
		if (laser_timer > 0.f) {
			draw_text_centered("LASER", 56.f, -42.5f, 0.32f, 0xF59E0B);
			draw_number((int)(laser_timer + 0.9f), 74.f, -42.5f, 0.32f, 0xF59E0B);
		}
	}

	for (int r = 0; r < BRK_ROWS; r++) {
		for (int c = 0; c < BRK_COLS; c++) {
			const Brick* b = &bricks[r][c];
			if (b->kind == BRICK_NONE) continue;
			u32 col = brick_color(b);
			draw_rect(b->x, b->y, brick_hx, brick_hy, col);
			if (b->kind == BRICK_TNT) {
				draw_text_centered("TNT", b->x, b->y, 0.28f, 0xFEF08A);
			} else if (b->kind == BRICK_STEEL) {
				draw_rect(b->x, b->y, brick_hx - 1.2f, brick_hy - 0.8f, 0x475569);
			}
		}
	}

	for (int i = 0; i < BRK_MAX_POWERUPS; i++) {
		if (!powerups[i].active) continue;
		u32 pcol = 0x22C55E;
		const char* tag = "W";
		if (powerups[i].kind == PWR_MULTIBALL) { pcol = 0xEC4899; tag = "M"; }
		else if (powerups[i].kind == PWR_LASER) { pcol = 0xF59E0B; tag = "L"; }
		else if (powerups[i].kind == PWR_LIFE)  { pcol = 0x38BDF8; tag = "+"; }
		draw_rect(powerups[i].x, powerups[i].y, 2.7f, 2.1f, 0xF8FAFC);
		draw_rect(powerups[i].x, powerups[i].y, 2.4f, 1.8f, pcol);
		draw_text_centered(tag, powerups[i].x, powerups[i].y, 0.34f, 0x050814);
	}

	for (int i = 0; i < BRK_MAX_LASERS; i++) {
		if (lasers[i].active) draw_rect(lasers[i].x, lasers[i].y, 0.45f, 1.6f, 0xFACC15);
	}

	for (int i = 0; i < BRK_MAX_PARTICLES; i++) {
		if (particles[i].life > 0.f) {
			float a = particles[i].life / particles[i].max_life;
			draw_rect_alpha(particles[i].x, particles[i].y, 0.65f, 0.65f, particles[i].color, a);
		}
	}

	u32 pad_col = (laser_timer > 0.f) ? 0xF59E0B : 0x38BDF8;
	draw_rect(paddle_x, paddle_y, paddle_hx, paddle_hy, pad_col);
	if (laser_timer > 0.f) {
		draw_rect(paddle_x - paddle_hx + 1.f, paddle_y + paddle_hy + 0.5f, 0.5f, 0.7f, 0xFACC15);
		draw_rect(paddle_x + paddle_hx - 1.f, paddle_y + paddle_hy + 0.5f, 0.5f, 0.7f, 0xFACC15);
	}

	for (int i = 0; i < BRK_MAX_BALLS; i++) {
		if (balls[i].active) {
			draw_rect(balls[i].x, balls[i].y, ball_r, ball_r, (i == 0) ? 0xF8FAFC : 0x22D3EE);
		}
	}
}

void
init() {
	game_state = STATE_MENU;
	menu_sel = 0;
	level_sel = 0;
	pause_sel = 0;
	shake_timer = 0.f;
	render_offset_x = 0.f;
	render_offset_y = 0.f;
	for (int i = 0; i < BRK_MAX_PARTICLES; i++) particles[i].life = 0.f;
	load_level(0);
	game_state = STATE_MENU;
}

Game_Result
update(Input* input, float dt) {
	if (pressed(BUTTON_M)) {
		set_audio_muted(!is_audio_muted());
		if (!is_audio_muted()) sfx(580, 40, 0.16f);
	}

	if (shake_timer > 0.f) {
		shake_timer -= dt;
		if (shake_timer <= 0.f) {
			render_offset_x = 0.f;
			render_offset_y = 0.f;
		} else {
			float phase = shake_timer * 90.f;
			render_offset_x = sinf(phase) * shake_mag * (shake_timer / 0.20f);
			render_offset_y = cosf(phase * 1.3f) * shake_mag * (shake_timer / 0.20f);
		}
	}

	for (int i = 0; i < BRK_MAX_PARTICLES; i++) {
		if (particles[i].life > 0.f) {
			particles[i].x += particles[i].vx * dt;
			particles[i].y += particles[i].vy * dt;
			particles[i].life -= dt;
		}
	}

	if (game_state == STATE_CONTROLS) {
		int tab = 1;
		if (update_and_draw_keybinds_ui(input, &tab, false,
		                                0x0B1120, 0x0F172A, 0x334155,
		                                0x162033, 0xF1F5F9, 0x64748B, 0x22D3EE)) {
			game_state = controls_return;
		}
		return GAME_CONTINUE;
	}

	if (game_state == STATE_SOUND) {
		if (update_and_draw_audio_ui(input,
		                             0x0B1120, 0x0F172A, 0x334155,
		                             0x162033, 0xF1F5F9, 0x64748B, 0x22D3EE)) {
			game_state = controls_return;
		}
		return GAME_CONTINUE;
	}

	if (game_state == STATE_MENU) {
		if (pressed(BUTTON_UP) || pressed(BUTTON_W))   { menu_sel = (menu_sel + 4) % 5; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_DOWN) || pressed(BUTTON_S)) { menu_sel = (menu_sel + 1) % 5; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE)) {
			if (menu_sel == 0) start_campaign(0);
			else if (menu_sel == 1) { level_sel = 0; game_state = STATE_LEVEL_SELECT; }
			else if (menu_sel == 2) { controls_return = STATE_MENU; game_state = STATE_CONTROLS; }
			else if (menu_sel == 3) { controls_return = STATE_MENU; game_state = STATE_SOUND; }
			else {
				render_offset_x = 0.f; render_offset_y = 0.f;
				return GAME_EXIT_TO_HUB;
			}
			return GAME_CONTINUE;
		}
		if (pressed(BUTTON_ESC)) {
			render_offset_x = 0.f; render_offset_y = 0.f;
			return GAME_EXIT_TO_HUB;
		}

		draw_world(false);
		draw_rect_alpha(0.f, 0.f, arena_half_x, arena_half_y, 0x050814, 0.78f);
		draw_text_centered("NEON BREAKER", 0.f, 27.f, 1.20f, 0x22D3EE);
		draw_text_centered("PLAY CAMPAIGN", 0.f,  9.f, 0.52f, menu_sel == 0 ? 0x38BDF8 : 0xF1F5F9);
		draw_text_centered("LEVEL SELECT",  0.f,  1.f, 0.52f, menu_sel == 1 ? 0x38BDF8 : 0xF1F5F9);
		draw_text_centered("CONTROLS",      0.f, -7.f, 0.52f, menu_sel == 2 ? 0x38BDF8 : 0xF1F5F9);
		draw_text_centered("SOUND",         0.f,-15.f, 0.52f, menu_sel == 3 ? 0x38BDF8 : 0xF1F5F9);
		draw_text_centered("BACK TO HUB",   0.f,-23.f, 0.52f, menu_sel == 4 ? 0xEF4444 : 0xF1F5F9);
		draw_text_centered(">", -36.f, 9.f - menu_sel * 8.f, 0.52f, 0x22D3EE);
		draw_text_centered("MOUSE / KEYBOARD SUPPORTED   ESC: HUB   M: MUTE", 0.f, -38.f, 0.32f, 0x64748B);
		return GAME_CONTINUE;
	}

	if (game_state == STATE_LEVEL_SELECT) {
		if (pressed(BUTTON_UP) || pressed(BUTTON_W))   { level_sel = (level_sel + BRK_NUM_LEVELS - 1) % BRK_NUM_LEVELS; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_DOWN) || pressed(BUTTON_S)) { level_sel = (level_sel + 1) % BRK_NUM_LEVELS; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_ESC)) { game_state = STATE_MENU; return GAME_CONTINUE; }
		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE)) {
			start_campaign(level_sel);
			return GAME_CONTINUE;
		}

		draw_world(false);
		draw_rect_alpha(0.f, 0.f, arena_half_x, arena_half_y, 0x050814, 0.84f);
		draw_text_centered("SELECT LEVEL", 0.f, 34.f, 0.90f, 0x22D3EE);
		for (int i = 0; i < BRK_NUM_LEVELS; i++) {
			float y = 21.f - i * 6.5f;
			u32 col = (i == level_sel) ? 0x38BDF8 : 0x94A3B8;
			draw_text_centered(LEVEL_NAMES[i], 0.f, y, 0.44f, col);
		}
		draw_text_centered(">", -44.f, 21.f - level_sel * 6.5f, 0.44f, 0x22D3EE);
		draw_text_centered("ENTER: PLAY   ESC: BACK", 0.f, -38.f, 0.34f, 0x64748B);
		return GAME_CONTINUE;
	}

	if (game_state == STATE_PAUSED) {
		if (vk_pressed(input, breaker_binds.pause) || pressed(BUTTON_P)) { game_state = STATE_PLAYING; return GAME_CONTINUE; }
		if (pressed(BUTTON_UP) || pressed(BUTTON_W))   { pause_sel = (pause_sel + 4) % 5; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_DOWN) || pressed(BUTTON_S)) { pause_sel = (pause_sel + 1) % 5; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE)) {
			if (pause_sel == 0) game_state = STATE_PLAYING;
			else if (pause_sel == 1) load_level(current_level);
			else if (pause_sel == 2) { controls_return = STATE_PAUSED; game_state = STATE_CONTROLS; }
			else if (pause_sel == 3) { controls_return = STATE_PAUSED; game_state = STATE_SOUND; }
			else game_state = STATE_MENU;
			return GAME_CONTINUE;
		}
		draw_world(true);
		draw_rect_alpha(0.f, 0.f, arena_half_x, arena_half_y, 0x050814, 0.75f);
		draw_text_centered("PAUSED",        0.f,  24.f, 1.0f, 0x22D3EE);
		draw_text_centered("RESUME",        0.f,   9.f, 0.52f, pause_sel == 0 ? 0x38BDF8 : 0xF1F5F9);
		draw_text_centered("RESTART LEVEL", 0.f,   1.f, 0.52f, pause_sel == 1 ? 0x38BDF8 : 0xF1F5F9);
		draw_text_centered("CONTROLS",      0.f,  -7.f, 0.52f, pause_sel == 2 ? 0x38BDF8 : 0xF1F5F9);
		draw_text_centered("SOUND",         0.f, -15.f, 0.52f, pause_sel == 3 ? 0x38BDF8 : 0xF1F5F9);
		draw_text_centered("MAIN MENU",     0.f, -23.f, 0.52f, pause_sel == 4 ? 0xF59E0B : 0xF1F5F9);
		draw_text_centered(">", -36.f, 9.f - pause_sel * 8.f, 0.52f, 0x22D3EE);
		return GAME_CONTINUE;
	}

	if (game_state == STATE_LEVEL_CLEAR) {
		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE)) {
			load_level(current_level + 1);
			return GAME_CONTINUE;
		}
		draw_world(true);
		draw_rect_alpha(0.f, 0.f, arena_half_x, arena_half_y, 0x050814, 0.78f);
		draw_text_centered("LEVEL CLEARED!", 0.f, 14.f, 1.0f, 0x22C55E);
		draw_text_centered(LEVEL_NAMES[current_level], 0.f, 2.f, 0.50f, 0xF1F5F9);
		draw_text_centered("PRESS SPACE FOR NEXT LEVEL", 0.f, -14.f, 0.42f, 0x22D3EE);
		return GAME_CONTINUE;
	}

	if (game_state == STATE_GAME_OVER || game_state == STATE_VICTORY) {
		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE)) {
			start_campaign(game_state == STATE_VICTORY ? 0 : current_level);
			return GAME_CONTINUE;
		}
		if (pressed(BUTTON_ESC)) { game_state = STATE_MENU; return GAME_CONTINUE; }
		draw_world(true);
		draw_rect_alpha(0.f, 0.f, arena_half_x, arena_half_y, 0x050814, 0.82f);
		bool won = (game_state == STATE_VICTORY);
		draw_text_centered(won ? "ARCADE CONQUERED!" : "GAME OVER", 0.f, 16.f, 1.0f, won ? 0xFACC15 : 0xEF4444);
		draw_text_centered("FINAL SCORE", 0.f, 2.f, 0.45f, 0x64748B);
		draw_number(score, 0.f, -6.f, 0.65f, 0xF8FAFC);
		draw_text_centered("SPACE: RETRY   ESC: MENU", 0.f, -20.f, 0.40f, 0x22D3EE);
		return GAME_CONTINUE;
	}

	if (vk_pressed(input, breaker_binds.pause) || pressed(BUTTON_P)) {
		pause_sel = 0;
		game_state = STATE_PAUSED;
		return GAME_CONTINUE;
	}

	if (wide_timer > 0.f) {
		wide_timer -= dt;
		if (wide_timer <= 0.f) paddle_hx = base_paddle_hx;
	}
	if (laser_timer > 0.f) laser_timer -= dt;
	if (laser_cd > 0.f) laser_cd -= dt;

	for (int r = 0; r < BRK_ROWS; r++) {
		for (int c = 0; c < BRK_COLS; c++) {
			if (bricks[r][c].flash > 0.f) bricks[r][c].flash = clampf(0.f, bricks[r][c].flash - dt * 6.f, 1.f);
		}
	}

	if (breaker_binds.mouse_aim && input->mouse_moved) {
		paddle_x = clampf(-arena_half_x + paddle_hx, input->mouse_x, arena_half_x - paddle_hx);
		paddle_dx = 0.f;
	} else {
		float ddp = 0.f;
		if (vk_down(input, breaker_binds.left)  || is_down(BUTTON_LEFT))  ddp -= 2400.f;
		if (vk_down(input, breaker_binds.right) || is_down(BUTTON_RIGHT)) ddp += 2400.f;
		ddp -= paddle_dx * 12.f;
		paddle_x += paddle_dx * dt + ddp * dt * dt * 0.5f;
		paddle_dx += ddp * dt;
		if (paddle_x + paddle_hx > arena_half_x) { paddle_x = arena_half_x - paddle_hx; paddle_dx = 0.f; }
		if (paddle_x - paddle_hx < -arena_half_x) { paddle_x = -arena_half_x + paddle_hx; paddle_dx = 0.f; }
	}

	if (vk_pressed(input, breaker_binds.action) || pressed(BUTTON_ENTER) ||
	    (breaker_binds.mouse_aim && pressed(BUTTON_MOUSE_LEFT))) {
		bool released = false;
		for (int i = 0; i < BRK_MAX_BALLS; i++) {
			if (balls[i].active && balls[i].stuck) {
				balls[i].stuck = false;
				released = true;
			}
		}
		if (!released && laser_timer > 0.f && laser_cd <= 0.f) {
			laser_cd = 0.25f;
			int spawned = 0;
			for (int i = 0; i < BRK_MAX_LASERS && spawned < 2; i++) {
				if (!lasers[i].active) {
					lasers[i].active = true;
					lasers[i].x = paddle_x + (spawned == 0 ? -paddle_hx + 1.2f : paddle_hx - 1.2f);
					lasers[i].y = paddle_y + paddle_hy + 1.f;
					spawned++;
				}
			}
			sfx(920, 35, 0.16f);
		}
	}

	for (int i = 0; i < BRK_MAX_LASERS; i++) {
		if (!lasers[i].active) continue;
		lasers[i].y += 130.f * dt;
		if (lasers[i].y > arena_half_y) { lasers[i].active = false; continue; }
		for (int r = 0; r < BRK_ROWS && lasers[i].active; r++) {
			for (int c = 0; c < BRK_COLS && lasers[i].active; c++) {
				if (bricks[r][c].kind == BRICK_NONE) continue;
				if (absf(lasers[i].x - bricks[r][c].x) < brick_hx + 0.45f &&
				    absf(lasers[i].y - bricks[r][c].y) < brick_hy + 1.6f) {
					lasers[i].active = false;
					damage_brick(r, c);
				}
			}
		}
	}

	for (int i = 0; i < BRK_MAX_POWERUPS; i++) {
		if (!powerups[i].active) continue;
		powerups[i].y -= 28.f * dt;
		if (powerups[i].y < -arena_half_y) { powerups[i].active = false; continue; }
		if (absf(powerups[i].x - paddle_x) < paddle_hx + 2.7f &&
		    absf(powerups[i].y - paddle_y) < paddle_hy + 2.1f) {
			sfx(840, 75, 0.22f);
			u32 pcol = 0x22C55E;
			if (powerups[i].kind == PWR_WIDE) {
				paddle_hx = 16.5f;
				wide_timer = 10.f;
				paddle_x = clampf(-arena_half_x + paddle_hx, paddle_x, arena_half_x - paddle_hx);
			} else if (powerups[i].kind == PWR_LASER) {
				pcol = 0xF59E0B;
				laser_timer = 10.f;
			} else if (powerups[i].kind == PWR_LIFE) {
				pcol = 0x38BDF8;
				if (lives < 9) lives++;
			} else if (powerups[i].kind == PWR_MULTIBALL) {
				pcol = 0xEC4899;
				float ref_speed = 76.f + (float)current_level * 3.5f;
				float ref_x = paddle_x;
				float ref_y = paddle_y + paddle_hy + ball_r + 0.5f;
				for (int b = 0; b < BRK_MAX_BALLS; b++) {
					if (balls[b].active) {
						ref_speed = balls[b].speed;
						if (!balls[b].stuck) { ref_x = balls[b].x; ref_y = balls[b].y; }
						break;
					}
				}
				int dir_idx = 0;
				for (int b = 0; b < BRK_MAX_BALLS; b++) {
					if (!balls[b].active) {
						balls[b].active = true;
						balls[b].stuck = false;
						balls[b].x = ref_x;
						balls[b].y = ref_y;
						balls[b].speed = ref_speed;
						float sx = (dir_idx == 0) ? -0.55f : (dir_idx == 1 ? 0.55f : 0.25f);
						balls[b].dx = sx * ref_speed;
						balls[b].dy = sqrtf(1.f - sx * sx) * ref_speed;
						dir_idx++;
					}
				}
			}
			spawn_particles(paddle_x, paddle_y, pcol, 10);
			powerups[i].active = false;
		}
	}

	int active_balls = 0;
	for (int b = 0; b < BRK_MAX_BALLS; b++) {
		if (!balls[b].active) continue;
		active_balls++;

		if (balls[b].stuck) {
			balls[b].x = paddle_x;
			balls[b].y = paddle_y + paddle_hy + ball_r + 0.1f;
			continue;
		}

		balls[b].x += balls[b].dx * dt;
		balls[b].y += balls[b].dy * dt;

		if (balls[b].x + ball_r > arena_half_x)  { balls[b].x = arena_half_x - ball_r;  balls[b].dx *= -1.f; sfx(320, 25, 0.12f); }
		if (balls[b].x - ball_r < -arena_half_x) { balls[b].x = -arena_half_x + ball_r; balls[b].dx *= -1.f; sfx(320, 25, 0.12f); }
		if (balls[b].y + ball_r > arena_half_y)  { balls[b].y = arena_half_y - ball_r;  balls[b].dy *= -1.f; sfx(320, 25, 0.12f); }

		if (balls[b].dy < 0.f &&
		    balls[b].x + ball_r > paddle_x - paddle_hx &&
		    balls[b].x - ball_r < paddle_x + paddle_hx &&
		    balls[b].y - ball_r < paddle_y + paddle_hy &&
		    balls[b].y + ball_r > paddle_y - paddle_hy) {
			balls[b].y = paddle_y + paddle_hy + ball_r;
			float hit = clampf(-1.f, (balls[b].x - paddle_x) / paddle_hx, 1.f);
			float angle = hit * 1.15f;
			balls[b].dx = sinf(angle) * balls[b].speed;
			balls[b].dy = cosf(angle) * balls[b].speed;
			combo = 0;
			sfx(460, 40, 0.18f);
		}

		for (int r = 0; r < BRK_ROWS; r++) {
			for (int c = 0; c < BRK_COLS; c++) {
				Brick* br = &bricks[r][c];
				if (br->kind == BRICK_NONE) continue;
				float dx = balls[b].x - br->x;
				float dy = balls[b].y - br->y;
				float ox = (brick_hx + ball_r) - absf(dx);
				float oy = (brick_hy + ball_r) - absf(dy);
				if (ox > 0.f && oy > 0.f) {
					if (ox < oy) {
						balls[b].x += (dx > 0.f ? ox : -ox);
						balls[b].dx *= -1.f;
					} else {
						balls[b].y += (dy > 0.f ? oy : -oy);
						balls[b].dy *= -1.f;
					}
					damage_brick(r, c);
					goto brick_hit_done;
				}
			}
		}
brick_hit_done:

		if (balls[b].y - ball_r < -arena_half_y) {
			balls[b].active = false;
			active_balls--;
		}
	}

	if (active_balls <= 0) {
		lives--;
		sfx(180, 160, 0.25f);
		if (lives <= 0) {
			game_state = STATE_GAME_OVER;
			return GAME_CONTINUE;
		}
		reset_ball_on_paddle();
	}

	if (count_breakable_bricks() == 0) {
		sfx(780, 220, 0.25f);
		if (current_level + 1 >= BRK_NUM_LEVELS) game_state = STATE_VICTORY;
		else game_state = STATE_LEVEL_CLEAR;
		return GAME_CONTINUE;
	}

	draw_world(true);
	return GAME_CONTINUE;
}

} // namespace Breaker


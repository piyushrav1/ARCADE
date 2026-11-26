#include <math.h>
#include "utils.h"
#include "renderer.h"
#include "keybinds.h"
#include "settings_ui.h"
#include "themes.h"
#include "save.h"
#include "game_interface.h"

#define pressed(b) (input->buttons[b].is_down && input->buttons[b].changed)

namespace Survivor {

enum Game_State {
	STATE_INTRO,
	STATE_MENU,
	STATE_SETTINGS,
	STATE_PLAYING,
	STATE_UPGRADE,
	STATE_PAUSED,
	STATE_GAME_OVER,
	STATE_VICTORY
};

enum Enemy_Kind {
	ENM_SWARMER = 0,
	ENM_TANK = 1,
	ENM_DASHER = 2,
	ENM_BOSS = 3
};

enum Upgrade_Kind {
	UPG_MULTISHOT = 0,
	UPG_FIRE_RATE = 1,
	UPG_ORBIT = 2,
	UPG_NOVA = 3,
	UPG_PIERCE = 4,
	UPG_SPEED_MAG = 5,
	UPG_REPAIR = 6,
	UPG_COUNT = 7
};

struct Bullet {
	bool active;
	float x, y, vx, vy;
	int pierce;
	int last_hit_idx;
	float life;
};

struct Enemy_Bullet {
	bool active;
	float x, y, vx, vy;
	float life;
};

struct Enemy {
	bool active;
	Enemy_Kind kind;
	float x, y, vx, vy;
	float hp, max_hp;
	float half_size;
	float flash;
	float blade_cd;
	float ai_timer;
	bool dashing;
};

struct Xp_Gem {
	bool active;
	float x, y;
	int value;
};

struct Particle {
	float x, y, vx, vy, life, max_life;
	u32 color;
};

#define SURV_MAX_BULLETS  48
#define SURV_MAX_EBULLETS 32
#define SURV_MAX_ENEMIES  56
#define SURV_MAX_GEMS     64
#define SURV_MAX_PARTICLES 64

global_variable const float arena_hx = 84.f;
global_variable const float arena_min_y = -44.f;
global_variable const float arena_max_y = 33.5f;

global_variable Game_State game_state = STATE_INTRO;
global_variable Game_State settings_return = STATE_MENU;
global_variable float intro_timer = 0.f;
global_variable int   intro_stage = 0;
global_variable float menu_anim_t = 0.f;
global_variable int   menu_sel = 0;
global_variable int   pause_sel = 0;

// Player stats
global_variable float px = 0.f, py = -5.f;
global_variable float p_hp = 100.f, p_max_hp = 100.f;
global_variable float p_speed = 32.f;
global_variable float p_invuln = 0.f;
global_variable float dash_timer = 0.f, dash_cd = 0.f;
global_variable float dash_vx = 0.f, dash_vy = 0.f;

global_variable int   xp = 0, xp_next = 10, player_lvl = 1;
global_variable int   score = 0;
global_variable int   wave = 1;
global_variable float run_time = 0.f;
global_variable float spawn_timer = 0.f;
global_variable bool  boss_spawned = false;

// Weapons
global_variable int   multishot = 1;
global_variable float fire_interval = 0.42f;
global_variable float fire_cd = 0.f;
global_variable int   bullet_pierce = 0;
global_variable int   orbit_blades = 0;
global_variable float orbit_angle = 0.f;
global_variable int   nova_level = 0;
global_variable float nova_timer = 0.f;
global_variable float nova_ring_r = 0.f;
global_variable float magnet_radius = 18.f;

// Upgrade draft
global_variable int upgrade_choices[3] = { 0, 1, 2 };
global_variable int upgrade_sel = 0;

global_variable Bullet       bullets[SURV_MAX_BULLETS];
global_variable Enemy_Bullet ebullets[SURV_MAX_EBULLETS];
global_variable Enemy        enemies[SURV_MAX_ENEMIES];
global_variable Xp_Gem       gems[SURV_MAX_GEMS];
global_variable Particle     particles[SURV_MAX_PARTICLES];

global_variable float shake_timer = 0.f, shake_mag = 0.f;
global_variable u32   rng_state = 9191u;

internal u32
next_rand() {
	rng_state = rng_state * 1664525u + 1013904223u;
	return rng_state;
}

internal void
sfx(int freq_hz, int duration_ms, float vol = 0.20f, int freq_end_hz = 0) {
	if (!is_audio_muted()) play_tone(freq_hz, duration_ms, vol, (float)freq_end_hz);
}

internal bool
in_rect(float x, float y, float cx, float cy, float hw, float hh) {
	return absf(x - cx) <= hw && absf(y - cy) <= hh;
}

internal void
spawn_particles(float x, float y, u32 color, int count) {
	int spawned = 0;
	for (int i = 0; i < SURV_MAX_PARTICLES && spawned < count; i++) {
		if (particles[i].life <= 0.f) {
			float angle = (float)((spawned * 137 + i * 29) % 360) * 0.0174533f;
			float speed = 18.f + (float)((i * 19) % 42);
			particles[i].x = x;
			particles[i].y = y;
			particles[i].vx = cosf(angle) * speed;
			particles[i].vy = sinf(angle) * speed;
			particles[i].max_life = 0.42f;
			particles[i].life = 0.42f;
			particles[i].color = color;
			spawned++;
		}
	}
}

internal void
roll_upgrade_choices() {
	int pool[UPG_COUNT];
	for (int i = 0; i < UPG_COUNT; i++) pool[i] = i;
	for (int i = UPG_COUNT - 1; i > 0; i--) {
		int j = (int)((next_rand() >> 16) % (u32)(i + 1));
		int tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
	}
	upgrade_choices[0] = pool[0];
	upgrade_choices[1] = pool[1];
	upgrade_choices[2] = pool[2];
	upgrade_sel = 0;
}

internal void
apply_upgrade(int kind) {
	if (kind == UPG_MULTISHOT)      { if (multishot < 5) multishot++; }
	else if (kind == UPG_FIRE_RATE) { fire_interval = clampf(0.12f, fire_interval * 0.78f, 0.50f); }
	else if (kind == UPG_ORBIT)     { if (orbit_blades < 4) orbit_blades++; }
	else if (kind == UPG_NOVA)      { nova_level++; }
	else if (kind == UPG_PIERCE)    { if (bullet_pierce < 4) bullet_pierce++; }
	else if (kind == UPG_SPEED_MAG) { p_speed = clampf(32.f, p_speed * 1.15f, 56.f); magnet_radius += 7.f; }
	else if (kind == UPG_REPAIR)    { p_max_hp += 25.f; p_hp = clampf(0.f, p_hp + 40.f, p_max_hp); }
	sfx(740, 120, 0.24f, 1120);
}

internal const char*
upgrade_title(int k) {
	switch (k) {
		case UPG_MULTISHOT: return "MULTISHOT";
		case UPG_FIRE_RATE: return "OVERCLOCK";
		case UPG_ORBIT:     return "ORBIT BLADE";
		case UPG_NOVA:      return "NOVA PULSE";
		case UPG_PIERCE:    return "PIERCING";
		case UPG_SPEED_MAG: return "TURBO MAGNET";
		default:            return "NANO REPAIR";
	}
}

internal const char*
upgrade_desc(int k) {
	switch (k) {
		case UPG_MULTISHOT: return "+1 SHOT PER VOLLEY";
		case UPG_FIRE_RATE: return "+22% FIRE RATE";
		case UPG_ORBIT:     return "+1 PLASMA BLADE";
		case UPG_NOVA:      return "SHOCKWAVE RING";
		case UPG_PIERCE:    return "+1 ENEMY PIERCE";
		case UPG_SPEED_MAG: return "+15% SPD + MAGNET";
		default:            return "+25 MAX HP + HEAL";
	}
}

internal void
start_new_run() {
	px = 0.f; py = -5.f;
	p_max_hp = 100.f; p_hp = 100.f;
	p_speed = 32.f; p_invuln = 0.f;
	dash_timer = 0.f; dash_cd = 0.f;
	xp = 0; xp_next = 10; player_lvl = 1;
	score = 0; wave = 1; run_time = 0.f;
	spawn_timer = 0.6f; boss_spawned = false;

	multishot = 1; fire_interval = 0.42f; fire_cd = 0.f;
	bullet_pierce = 0; orbit_blades = 0; orbit_angle = 0.f;
	nova_level = 0; nova_timer = 0.f; nova_ring_r = 0.f;
	magnet_radius = 18.f;

	for (int i = 0; i < SURV_MAX_BULLETS; i++)   bullets[i].active = false;
	for (int i = 0; i < SURV_MAX_EBULLETS; i++)  ebullets[i].active = false;
	for (int i = 0; i < SURV_MAX_ENEMIES; i++)   enemies[i].active = false;
	for (int i = 0; i < SURV_MAX_GEMS; i++)      gems[i].active = false;
	for (int i = 0; i < SURV_MAX_PARTICLES; i++) particles[i].life = 0.f;

	game_state = STATE_PLAYING;
	sfx(520, 100, 0.22f, 880);
}

internal void
spawn_enemy(Enemy_Kind kind) {
	for (int i = 0; i < SURV_MAX_ENEMIES; i++) {
		if (!enemies[i].active) {
			Enemy* e = &enemies[i];
			e->active = true;
			e->kind = kind;
			e->flash = 0.f;
			e->blade_cd = 0.f;
			e->ai_timer = 1.2f;
			e->dashing = false;
			e->vx = 0.f; e->vy = 0.f;

			// Spawn along arena perimeter
			u32 side = (next_rand() >> 16) % 4;
			if (side == 0)      { e->x = -arena_hx + 2.f; e->y = (float)((s32)((next_rand() >> 16) % 70) - 38); }
			else if (side == 1) { e->x =  arena_hx - 2.f; e->y = (float)((s32)((next_rand() >> 16) % 70) - 38); }
			else if (side == 2) { e->x = (float)((s32)((next_rand() >> 16) % 160) - 80); e->y = arena_max_y - 2.f; }
			else                { e->x = (float)((s32)((next_rand() >> 16) % 160) - 80); e->y = arena_min_y + 2.f; }

			if (kind == ENM_SWARMER)    { e->max_hp = 2.f + wave * 0.5f; e->half_size = 1.6f; }
			else if (kind == ENM_TANK)  { e->max_hp = 7.f + wave * 2.0f; e->half_size = 2.6f; }
			else if (kind == ENM_DASHER){ e->max_hp = 3.f + wave * 0.8f; e->half_size = 1.8f; }
			else {
				e->x = 0.f; e->y = 22.f;
				e->max_hp = 75.f;
				e->half_size = 4.5f;
			}
			e->hp = e->max_hp;
			break;
		}
	}
}

internal void
damage_enemy(int idx, float dmg, float kbx, float kby, const Theme& th) {
	Enemy* e = &enemies[idx];
	if (!e->active) return;
	e->hp -= dmg;
	e->flash = 1.f;
	if (e->kind != ENM_TANK && e->kind != ENM_BOSS) {
		e->x = clampf(-arena_hx + e->half_size, e->x + kbx, arena_hx - e->half_size);
		e->y = clampf(arena_min_y + e->half_size, e->y + kby, arena_max_y - e->half_size);
	}
	if (e->hp <= 0.f) {
		e->active = false;
		score += (e->kind == ENM_BOSS) ? 1500 : (e->kind == ENM_TANK ? 80 : 25);
		spawn_particles(e->x, e->y, (e->kind == ENM_BOSS) ? 0xFACC15 : th.p2, (e->kind == ENM_BOSS) ? 32 : 8);
		sfx(e->kind == ENM_BOSS ? 160 : 480, e->kind == ENM_BOSS ? 240 : 35, 0.20f, e->kind == ENM_BOSS ? 50 : 720);

		if (e->kind == ENM_BOSS) {
			if (score > user_stats.survivor_high_score) user_stats.survivor_high_score = score;
			if (wave > user_stats.survivor_best_wave)   user_stats.survivor_best_wave = wave;
			save_user_data();
			game_state = STATE_VICTORY;
			return;
		}

		// Drop XP gem
		for (int g = 0; g < SURV_MAX_GEMS; g++) {
			if (!gems[g].active) {
				gems[g].active = true;
				gems[g].x = e->x;
				gems[g].y = e->y;
				gems[g].value = (e->kind == ENM_TANK) ? 6 : 2;
				break;
			}
		}
	}
}

internal void
draw_world(const Theme& th) {
	clear_screen(th.bg_outer);
	draw_rect(0.f, 0.f, 86.2f, 46.2f, th.arena_border);
	draw_rect(0.f, 0.f, 85.0f, 45.0f, th.arena_bg);

	// Top HUD divider
	draw_rect(0.f, 34.5f, 85.f, 0.25f, th.center_line);
	draw_text_centered("WAVE", -76.f, 40.f, 0.30f, th.text_dim);
	draw_number(wave, -65.f, 40.f, 0.34f, th.accent);

	draw_text_centered("LVL", -52.f, 40.f, 0.30f, th.text_dim);
	draw_number(player_lvl, -42.f, 40.f, 0.34f, 0xFACC15);

	draw_text_centered("SCORE", -22.f, 40.f, 0.30f, th.text_dim);
	draw_number(score, -2.f, 40.f, 0.34f, th.text);

	// HP Bar
	draw_text_centered("HP", 16.f, 40.f, 0.30f, th.text_dim);
	draw_rect(35.f, 40.f, 12.2f, 1.4f, th.panel_border);
	float hp_frac = clampf(0.f, p_hp / p_max_hp, 1.f);
	if (hp_frac > 0.f) {
		draw_rect(35.f - 12.f * (1.f - hp_frac), 40.f, 12.f * hp_frac, 1.1f, hp_frac > 0.3f ? 0x22C55E : 0xEF4444);
	}

	// XP Bar (thin bar below HUD)
	float xp_frac = clampf(0.f, (float)xp / (float)xp_next, 1.f);
	if (xp_frac > 0.f) {
		draw_rect(-85.f + 85.f * xp_frac, 35.1f, 85.f * xp_frac, 0.45f, 0x22D3EE);
	}

	// Dash indicator
	draw_text_centered(dash_cd <= 0.f ? "DASH:RDY" : "DASH:CD", 66.f, 40.f, 0.28f,
	                   dash_cd <= 0.f ? th.accent : th.text_dim);

	// Nova ring
	if (nova_ring_r > 0.f) {
		float alpha = clampf(0.f, 1.f - nova_ring_r / 26.f, 0.8f);
		draw_rect_alpha(px, py, nova_ring_r, 0.5f, th.accent, alpha);
		draw_rect_alpha(px, py, 0.5f, nova_ring_r, th.accent, alpha);
	}

	// XP Gems
	for (int i = 0; i < SURV_MAX_GEMS; i++) {
		if (gems[i].active) {
			draw_rect(gems[i].x, gems[i].y, 0.8f, 0.8f, gems[i].value > 2 ? 0xFACC15 : 0x22D3EE);
		}
	}

	// Enemies
	for (int i = 0; i < SURV_MAX_ENEMIES; i++) {
		const Enemy* e = &enemies[i];
		if (!e->active) continue;
		u32 col = th.p2;
		if (e->kind == ENM_TANK) col = 0xA855F7;
		else if (e->kind == ENM_DASHER) col = 0xEF4444;
		else if (e->kind == ENM_BOSS) col = 0xFACC15;
		col = lerp_color(col, 0xFFFFFF, e->flash);
		draw_rect(e->x, e->y, e->half_size, e->half_size, col);

		if (e->kind == ENM_BOSS) {
			float bf = clampf(0.f, e->hp / e->max_hp, 1.f);
			draw_rect(0.f, 30.5f, 32.f, 1.1f, th.panel_border);
			if (bf > 0.f) draw_rect(-32.f * (1.f - bf), 30.5f, 31.6f * bf, 0.8f, 0xEF4444);
		}
	}

	// Player bullets & Enemy bullets
	for (int i = 0; i < SURV_MAX_BULLETS; i++) {
		if (bullets[i].active) draw_rect(bullets[i].x, bullets[i].y, 0.7f, 0.7f, th.ball);
	}
	for (int i = 0; i < SURV_MAX_EBULLETS; i++) {
		if (ebullets[i].active) draw_rect(ebullets[i].x, ebullets[i].y, 0.9f, 0.9f, 0xEF4444);
	}

	// Orbiting blades
	for (int b = 0; b < orbit_blades; b++) {
		float a = orbit_angle + (float)b * (6.283185f / (float)orbit_blades);
		float bx = px + cosf(a) * 8.5f;
		float by = py + sinf(a) * 8.5f;
		draw_rect(bx, by, 1.2f, 1.2f, th.accent);
	}

	// Particles
	for (int i = 0; i < SURV_MAX_PARTICLES; i++) {
		if (particles[i].life > 0.f) {
			float a = particles[i].life / particles[i].max_life;
			draw_rect_alpha(particles[i].x, particles[i].y, 0.65f, 0.65f, particles[i].color, a);
		}
	}

	// Player
	u32 pcol = (p_invuln > 0.f && ((int)(p_invuln * 20.f) % 2 == 0)) ? 0xFFFFFF : th.p1;
	if (dash_timer > 0.f) pcol = th.accent;
	draw_rect(px, py, 2.0f, 2.0f, pcol);
}

void
init() {
	game_state = STATE_INTRO;
	intro_timer = 0.f;
	intro_stage = 0;
	menu_sel = 0;
	pause_sel = 0;
	shake_timer = 0.f;
	render_offset_x = 0.f;
	render_offset_y = 0.f;
	for (int i = 0; i < SURV_MAX_PARTICLES; i++) particles[i].life = 0.f;
}

Game_Result
update(Input* input, float dt) {
	const Theme& th = resolve_theme(user_stats.survivor_theme);

	set_cursor_visible(game_state != STATE_PLAYING && game_state != STATE_INTRO);

	if (game_state == STATE_SETTINGS) {
		if (update_and_draw_settings_ui(input, 2,
		                                th.bg_outer, th.panel_bg, th.panel_border,
		                                th.card_bg, th.text, th.text_dim, th.accent)) {
			game_state = settings_return;
		}
		return GAME_CONTINUE;
	}

	if (pressed(BUTTON_M)) {
		set_audio_muted(!is_audio_muted());
		if (!is_audio_muted()) sfx(580, 40, 0.16f);
		save_user_data();
	}

	if (shake_timer > 0.f) {
		shake_timer -= dt;
		if (shake_timer <= 0.f) {
			render_offset_x = 0.f; render_offset_y = 0.f;
		} else {
			float phase = shake_timer * 90.f;
			render_offset_x = sinf(phase) * shake_mag * (shake_timer / 0.20f);
			render_offset_y = cosf(phase * 1.3f) * shake_mag * (shake_timer / 0.20f);
		}
	}

	for (int i = 0; i < SURV_MAX_PARTICLES; i++) {
		if (particles[i].life > 0.f) {
			particles[i].x += particles[i].vx * dt;
			particles[i].y += particles[i].vy * dt;
			particles[i].life -= dt;
		}
	}

	// INTRO
	if (game_state == STATE_INTRO) {
		intro_timer += dt;
		if (intro_stage == 0) {
			intro_stage = 1;
			sfx(180, 180, 0.20f, 680);
		} else if (intro_stage == 1 && intro_timer >= 0.75f) {
			intro_stage = 2;
			sfx(520, 180, 0.24f, 1040);
			spawn_particles(0.f, 2.f, th.accent, 28);
		}

		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE) ||
		    pressed(BUTTON_ESC) || pressed(BUTTON_MOUSE_LEFT) || intro_timer >= 2.0f) {
			game_state = STATE_MENU;
			return GAME_CONTINUE;
		}

		clear_screen(th.bg_outer);
		draw_rect(0.f, 0.f, 86.2f, 46.2f, th.panel_border);
		draw_rect(0.f, 0.f, 85.0f, 45.0f, th.panel_bg);

		float r = fmodf(intro_timer * 35.f, 40.f);
		draw_rect_alpha(0.f, 0.f, r * 1.8f, r, th.accent, 0.22f);

		for (int i = 0; i < SURV_MAX_PARTICLES; i++) {
			if (particles[i].life > 0.f) {
				float a = particles[i].life / particles[i].max_life;
				draw_rect_alpha(particles[i].x, particles[i].y, 0.75f, 0.75f, particles[i].color, a);
			}
		}

		if (intro_timer >= 0.75f) {
			draw_text_centered("NEON SURVIVOR", 0.f, 4.f, 1.20f, th.accent);
			draw_text_centered("SURVIVE THE SWARM...", 0.f, -14.f, 0.40f, th.text);
		} else {
			draw_text_centered("SCANNING SECTOR...", 0.f, 0.f, 0.44f, th.text_dim);
		}
		return GAME_CONTINUE;
	}

	// MENU (Dedicated Backdrop — No Gameplay Behind Menu)
	if (game_state == STATE_MENU) {
		menu_anim_t += dt;
		const int MENU_ITEMS = 3;
		if (pressed(BUTTON_UP) || pressed(BUTTON_W))   { menu_sel = (menu_sel + MENU_ITEMS - 1) % MENU_ITEMS; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_DOWN) || pressed(BUTTON_S)) { menu_sel = (menu_sel + 1) % MENU_ITEMS; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_ESC)) {
			render_offset_x = 0.f; render_offset_y = 0.f;
			return GAME_EXIT_TO_HUB;
		}

		float mx = input->mouse_x;
		float my = input->mouse_y;
		bool clicked = pressed(BUTTON_MOUSE_LEFT);
		bool activate = pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE);
		for (int i = 0; i < MENU_ITEMS; i++) {
			float y = 5.f - i * 11.f;
			if (in_rect(mx, my, 0.f, y, 44.f, 3.6f)) {
				if ((input->mouse_moved || clicked) && menu_sel != i) {
					menu_sel = i;
					sfx(440, 24, 0.10f);
				}
				if (clicked) { menu_sel = i; activate = true; }
			}
		}

		if (activate) {
			if (menu_sel == 0) { start_new_run(); return GAME_CONTINUE; }
			else if (menu_sel == 1) { settings_return = STATE_MENU; game_state = STATE_SETTINGS; return GAME_CONTINUE; }
			else {
				render_offset_x = 0.f; render_offset_y = 0.f;
				return GAME_EXIT_TO_HUB;
			}
		}

		clear_screen(th.bg_outer);
		draw_rect(0.f, 0.f, 86.2f, 46.2f, th.panel_border);
		draw_rect(0.f, 0.f, 85.0f, 45.0f, th.panel_bg);
		for (float gx = -60.f; gx <= 60.f; gx += 30.f) draw_rect_alpha(gx, 0.f, 0.12f, 45.f, th.panel_border, 0.22f);
		for (float gy = -30.f; gy <= 30.f; gy += 15.f) draw_rect_alpha(0.f, gy, 85.f, 0.12f, th.panel_border, 0.22f);
		for (int s = 0; s < 16; s++) {
			float sx = -75.f + ((s * 47) % 150);
			float sy = 44.f - fmodf(menu_anim_t * (3.f + (s % 4) * 1.5f) + s * 6.f, 88.f);
			draw_rect_alpha(sx, sy, 0.35f, 0.35f, th.accent, 0.35f);
		}

		draw_text_centered("NEON SURVIVOR", 0.f, 34.f, 1.10f, th.accent);
		draw_rect(0.f, 25.f, 44.f, 0.22f, th.panel_border);

		if (user_stats.survivor_high_score > 0 || user_stats.survivor_best_wave > 0) {
			draw_text_centered("BEST SCORE:", -24.f, 20.f, 0.30f, th.text_dim);
			draw_number(user_stats.survivor_high_score, -2.f, 20.f, 0.32f, 0xFACC15);
			draw_text_centered("WAVE:", 22.f, 20.f, 0.30f, th.text_dim);
			draw_number(user_stats.survivor_best_wave, 36.f, 20.f, 0.32f, 0x22C55E);
		}

		const char* mlabels[3] = { "PLAY SURVIVAL", "SETTINGS", "BACK TO HUB" };
		for (int i = 0; i < MENU_ITEMS; i++) {
			float y = 5.f - i * 11.f;
			bool sel = (i == menu_sel);
			draw_rect(0.f, y, 44.4f, 3.8f, sel ? th.accent : th.panel_border);
			draw_rect(0.f, y, 44.0f, 3.4f, sel ? th.card_selected : th.card_bg);
			draw_text_centered(mlabels[i], 0.f, y, 0.46f, sel ? (i == 2 ? 0xEF4444 : th.text) : th.text_dim);
		}
		draw_text_centered("AUTO-FIRE + WASD/MOUSE + DASH   ESC: HUB", 0.f, -40.f, 0.30f, th.text_dim);
		return GAME_CONTINUE;
	}

	// UPGRADE DRAFT (3 Cards)
	if (game_state == STATE_UPGRADE) {
		if (pressed(BUTTON_LEFT)  || pressed(BUTTON_A)) { upgrade_sel = (upgrade_sel + 2) % 3; sfx(440, 30, 0.14f); }
		if (pressed(BUTTON_RIGHT) || pressed(BUTTON_D)) { upgrade_sel = (upgrade_sel + 1) % 3; sfx(440, 30, 0.14f); }

		float mx = input->mouse_x;
		float my = input->mouse_y;
		bool pick = pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE);
		if (vk_pressed(input, '1')) { upgrade_sel = 0; pick = true; }
		if (vk_pressed(input, '2')) { upgrade_sel = 1; pick = true; }
		if (vk_pressed(input, '3')) { upgrade_sel = 2; pick = true; }

		for (int c = 0; c < 3; c++) {
			float cx = -44.f + c * 44.f;
			if (in_rect(mx, my, cx, -2.f, 19.f, 18.f)) {
				if (input->mouse_moved && upgrade_sel != c) {
					upgrade_sel = c;
					sfx(440, 24, 0.10f);
				}
				if (pressed(BUTTON_MOUSE_LEFT)) { upgrade_sel = c; pick = true; }
			}
		}

		if (pick) {
			apply_upgrade(upgrade_choices[upgrade_sel]);
			game_state = STATE_PLAYING;
			return GAME_CONTINUE;
		}

		draw_world(th);
		draw_rect_alpha(0.f, 0.f, 85.f, 45.f, 0x020408, 0.80f);
		draw_text_centered("LEVEL UP! CHOOSE UPGRADE", 0.f, 27.f, 0.68f, 0xFACC15);

		for (int c = 0; c < 3; c++) {
			float cx = -44.f + c * 44.f;
			bool sel = (c == upgrade_sel);
			int k = upgrade_choices[c];
			draw_rect(cx, -2.f, 19.4f, 18.4f, sel ? 0xFACC15 : th.panel_border);
			draw_rect(cx, -2.f, 19.0f, 18.0f, sel ? th.card_selected : th.card_bg);
			draw_number(c + 1, cx, 10.f, 0.55f, sel ? 0xFACC15 : th.text_dim);
			draw_text_centered(upgrade_title(k), cx, -1.f, 0.36f, sel ? th.text : th.text_dim);
			draw_text_centered(upgrade_desc(k), cx, -10.f, 0.26f, th.accent);
		}
		return GAME_CONTINUE;
	}

	// PAUSED
	if (game_state == STATE_PAUSED) {
		const int PAUSE_ITEMS = 4;
		if (vk_pressed(input, survivor_binds.pause) || pressed(BUTTON_P)) { game_state = STATE_PLAYING; return GAME_CONTINUE; }
		if (pressed(BUTTON_UP) || pressed(BUTTON_W))   { pause_sel = (pause_sel + PAUSE_ITEMS - 1) % PAUSE_ITEMS; sfx(440, 35, 0.15f); }
		if (pressed(BUTTON_DOWN) || pressed(BUTTON_S)) { pause_sel = (pause_sel + 1) % PAUSE_ITEMS; sfx(440, 35, 0.15f); }

		float mx = input->mouse_x;
		float my = input->mouse_y;
		bool activate = pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE);
		for (int i = 0; i < PAUSE_ITEMS; i++) {
			float y = 8.f - i * 8.5f;
			if (in_rect(mx, my, 0.f, y, 36.f, 3.0f)) {
				if (input->mouse_moved && pause_sel != i) { pause_sel = i; sfx(440, 24, 0.10f); }
				if (pressed(BUTTON_MOUSE_LEFT)) { pause_sel = i; activate = true; }
			}
		}
		if (activate) {
			if (pause_sel == 0) game_state = STATE_PLAYING;
			else if (pause_sel == 1) start_new_run();
			else if (pause_sel == 2) { settings_return = STATE_PAUSED; game_state = STATE_SETTINGS; }
			else game_state = STATE_MENU;
			return GAME_CONTINUE;
		}

		draw_world(th);
		draw_rect_alpha(0.f, 0.f, 85.f, 45.f, 0x050814, 0.78f);
		draw_text_centered("PAUSED", 0.f, 24.f, 1.0f, th.accent);
		const char* plabels[4] = { "RESUME", "RESTART", "SETTINGS", "MAIN MENU" };
		for (int i = 0; i < PAUSE_ITEMS; i++) {
			float y = 8.f - i * 8.5f;
			bool sel = (i == pause_sel);
			draw_rect(0.f, y, 36.4f, 3.2f, sel ? th.accent : th.panel_border);
			draw_rect(0.f, y, 36.0f, 2.8f, sel ? th.card_selected : th.card_bg);
			draw_text_centered(plabels[i], 0.f, y, 0.46f, sel ? (i == 3 ? th.p2 : th.text) : th.text_dim);
		}
		return GAME_CONTINUE;
	}

	// GAME OVER / VICTORY
	if (game_state == STATE_GAME_OVER || game_state == STATE_VICTORY) {
		if (pressed(BUTTON_ENTER) || pressed(BUTTON_SPACE) || pressed(BUTTON_MOUSE_LEFT)) {
			start_new_run();
			return GAME_CONTINUE;
		}
		if (pressed(BUTTON_ESC)) { game_state = STATE_MENU; return GAME_CONTINUE; }

		draw_world(th);
		draw_rect_alpha(0.f, 0.f, 85.f, 45.f, 0x050814, 0.82f);
		bool won = (game_state == STATE_VICTORY);
		draw_text_centered(won ? "SECTOR CLEARED!" : "OVERRUN!", 0.f, 18.f, 1.0f, won ? 0xFACC15 : 0xEF4444);
		draw_text_centered("FINAL SCORE", 0.f, 4.f, 0.42f, th.text_dim);
		draw_number(score, 0.f, -4.f, 0.65f, th.text);
		draw_text_centered("CLICK/SPACE: RETRY   ESC: MENU", 0.f, -20.f, 0.38f, th.accent);
		return GAME_CONTINUE;
	}

	// -----------------------------------------------------------------------
	// STATE_PLAYING
	// -----------------------------------------------------------------------
	if (vk_pressed(input, survivor_binds.pause) || pressed(BUTTON_P)) {
		pause_sel = 0;
		game_state = STATE_PAUSED;
		return GAME_CONTINUE;
	}

	run_time += dt;
	int new_wave = 1 + (int)(run_time / 20.f);
	if (new_wave > 5) new_wave = 5;
	if (new_wave != wave) {
		wave = new_wave;
		sfx(660, 120, 0.22f, 990);
	}
	if (wave == 5 && !boss_spawned) {
		boss_spawned = true;
		spawn_enemy(ENM_BOSS);
		sfx(140, 260, 0.28f, 420);
	}

	if (p_invuln > 0.f) p_invuln -= dt;
	if (dash_cd > 0.f)  dash_cd -= dt;

	// Movement & Dash
	float mx_dir = 0.f, my_dir = 0.f;
	if (vk_down(input, survivor_binds.left)  || vk_down(input, VK_LEFT))  mx_dir -= 1.f;
	if (vk_down(input, survivor_binds.right) || vk_down(input, VK_RIGHT)) mx_dir += 1.f;
	if (vk_down(input, survivor_binds.up)    || vk_down(input, VK_UP))    my_dir += 1.f;
	if (vk_down(input, survivor_binds.down)  || vk_down(input, VK_DOWN))  my_dir -= 1.f;

	if (survivor_binds.mouse_move && mx_dir == 0.f && my_dir == 0.f) {
		float dx = input->mouse_x - px;
		float dy = clampf(arena_min_y, input->mouse_y, arena_max_y) - py;
		float d = sqrtf(dx * dx + dy * dy);
		if (d > 1.8f) { mx_dir = dx / d; my_dir = dy / d; }
	} else {
		float len = sqrtf(mx_dir * mx_dir + my_dir * my_dir);
		if (len > 0.01f) { mx_dir /= len; my_dir /= len; }
	}

	if ((vk_pressed(input, survivor_binds.dash) || vk_pressed(input, VK_SHIFT)) && dash_cd <= 0.f) {
		dash_timer = 0.16f;
		dash_cd = 1.6f;
		p_invuln = 0.22f;
		dash_vx = (mx_dir != 0.f || my_dir != 0.f) ? mx_dir * 115.f : 115.f;
		dash_vy = (mx_dir != 0.f || my_dir != 0.f) ? my_dir * 115.f : 0.f;
		spawn_particles(px, py, th.accent, 10);
		sfx(720, 65, 0.18f, 340);
	}

	if (dash_timer > 0.f) {
		dash_timer -= dt;
		px += dash_vx * dt;
		py += dash_vy * dt;
	} else {
		px += mx_dir * p_speed * dt;
		py += my_dir * p_speed * dt;
	}
	px = clampf(-arena_hx + 2.f, px, arena_hx - 2.f);
	py = clampf(arena_min_y + 2.f, py, arena_max_y - 2.f);

	// Auto-fire Pulse Blaster
	fire_cd -= dt;
	if (fire_cd <= 0.f) {
		int target = -1;
		float best_d2 = 1e9f;
		for (int i = 0; i < SURV_MAX_ENEMIES; i++) {
			if (!enemies[i].active) continue;
			float dx = enemies[i].x - px;
			float dy = enemies[i].y - py;
			float d2 = dx * dx + dy * dy;
			if (d2 < best_d2) { best_d2 = d2; target = i; }
		}
		if (target >= 0) {
			fire_cd = fire_interval;
			float base_ang = atan2f(enemies[target].y - py, enemies[target].x - px);
			float spread = 0.18f;
			for (int s = 0; s < multishot; s++) {
				float ang = base_ang + ((float)s - (float)(multishot - 1) * 0.5f) * spread;
				for (int b = 0; b < SURV_MAX_BULLETS; b++) {
					if (!bullets[b].active) {
						bullets[b].active = true;
						bullets[b].x = px;
						bullets[b].y = py;
						bullets[b].vx = cosf(ang) * 105.f;
						bullets[b].vy = sinf(ang) * 105.f;
						bullets[b].pierce = bullet_pierce;
						bullets[b].last_hit_idx = -1;
						bullets[b].life = 1.6f;
						break;
					}
				}
			}
			sfx(820, 22, 0.10f, 520);
		}
	}

	// Orbiting blades
	if (orbit_blades > 0) orbit_angle += 4.2f * dt;

	// Nova Pulse
	if (nova_level > 0) {
		nova_timer += dt;
		if (nova_ring_r > 0.f) {
			nova_ring_r += 55.f * dt;
			if (nova_ring_r >= 26.f) nova_ring_r = 0.f;
		}
		if (nova_timer >= 3.2f) {
			nova_timer = 0.f;
			nova_ring_r = 2.f;
			float nova_r2 = (22.f + nova_level * 3.f);
			nova_r2 *= nova_r2;
			for (int i = 0; i < SURV_MAX_ENEMIES; i++) {
				if (!enemies[i].active) continue;
				float dx = enemies[i].x - px;
				float dy = enemies[i].y - py;
				float d2 = dx * dx + dy * dy;
				if (d2 < nova_r2 && d2 > 0.01f) {
					float d = sqrtf(d2);
					damage_enemy(i, 2.5f + nova_level, (dx / d) * 6.f, (dy / d) * 6.f, th);
				}
			}
			sfx(280, 110, 0.18f, 140);
		}
	}

	// Spawn waves
	spawn_timer -= dt;
	if (spawn_timer <= 0.f) {
		spawn_timer = clampf(0.32f, 1.25f - wave * 0.18f, 1.25f);
		u32 r = (next_rand() >> 16) % 100;
		Enemy_Kind k = ENM_SWARMER;
		if (wave >= 2 && r < 22) k = ENM_TANK;
		else if (wave >= 3 && r >= 72) k = ENM_DASHER;
		spawn_enemy(k);
	}

	// Update bullets
	for (int b = 0; b < SURV_MAX_BULLETS; b++) {
		if (!bullets[b].active) continue;
		bullets[b].x += bullets[b].vx * dt;
		bullets[b].y += bullets[b].vy * dt;
		bullets[b].life -= dt;
		if (bullets[b].life <= 0.f || absf(bullets[b].x) > arena_hx ||
		    bullets[b].y < arena_min_y || bullets[b].y > arena_max_y) {
			bullets[b].active = false;
			continue;
		}
		for (int i = 0; i < SURV_MAX_ENEMIES; i++) {
			if (!enemies[i].active || bullets[b].last_hit_idx == i) continue;
			if (absf(bullets[b].x - enemies[i].x) < enemies[i].half_size + 0.7f &&
			    absf(bullets[b].y - enemies[i].y) < enemies[i].half_size + 0.7f) {
				bullets[b].last_hit_idx = i;
				damage_enemy(i, 2.0f, bullets[b].vx * 0.02f, bullets[b].vy * 0.02f, th);
				if (bullets[b].pierce > 0) bullets[b].pierce--;
				else { bullets[b].active = false; break; }
			}
		}
	}

	// Update enemy bullets
	for (int i = 0; i < SURV_MAX_EBULLETS; i++) {
		if (!ebullets[i].active) continue;
		ebullets[i].x += ebullets[i].vx * dt;
		ebullets[i].y += ebullets[i].vy * dt;
		ebullets[i].life -= dt;
		if (ebullets[i].life <= 0.f || absf(ebullets[i].x) > arena_hx ||
		    ebullets[i].y < arena_min_y || ebullets[i].y > arena_max_y) {
			ebullets[i].active = false;
			continue;
		}
		if (p_invuln <= 0.f && absf(ebullets[i].x - px) < 2.4f && absf(ebullets[i].y - py) < 2.4f) {
			ebullets[i].active = false;
			p_hp -= 12.f;
			p_invuln = 0.65f;
			shake_timer = 0.16f; shake_mag = 0.9f;
			sfx(180, 110, 0.24f, 90);
		}
	}

	// Update enemies
	for (int i = 0; i < SURV_MAX_ENEMIES; i++) {
		Enemy* e = &enemies[i];
		if (!e->active) continue;
		if (e->flash > 0.f)    e->flash = clampf(0.f, e->flash - dt * 6.f, 1.f);
		if (e->blade_cd > 0.f) e->blade_cd -= dt;

		float dx = px - e->x;
		float dy = py - e->y;
		float d = sqrtf(dx * dx + dy * dy);
		if (d > 0.01f) { dx /= d; dy /= d; }

		if (e->kind == ENM_SWARMER) {
			float spd = 16.f + wave * 1.8f;
			e->x += dx * spd * dt;
			e->y += dy * spd * dt;
		} else if (e->kind == ENM_TANK) {
			float spd = 9.f + wave * 0.8f;
			e->x += dx * spd * dt;
			e->y += dy * spd * dt;
		} else if (e->kind == ENM_DASHER) {
			e->ai_timer -= dt;
			if (!e->dashing && e->ai_timer <= 0.f) {
				e->dashing = true;
				e->ai_timer = 0.55f;
				e->vx = dx * 48.f;
				e->vy = dy * 48.f;
			} else if (e->dashing) {
				e->x += e->vx * dt;
				e->y += e->vy * dt;
				if (e->ai_timer <= 0.f) { e->dashing = false; e->ai_timer = 1.4f; }
			} else {
				e->x += dx * 10.f * dt;
				e->y += dy * 10.f * dt;
			}
		} else {
			// Boss
			e->x += dx * 7.5f * dt;
			e->y += dy * 7.5f * dt;
			e->ai_timer -= dt;
			if (e->ai_timer <= 0.f) {
				e->ai_timer = 2.0f;
				for (int s = 0; s < 8; s++) {
					float a = (float)s * (6.283185f / 8.f);
					for (int eb = 0; eb < SURV_MAX_EBULLETS; eb++) {
						if (!ebullets[eb].active) {
							ebullets[eb].active = true;
							ebullets[eb].x = e->x;
							ebullets[eb].y = e->y;
							ebullets[eb].vx = cosf(a) * 34.f;
							ebullets[eb].vy = sinf(a) * 34.f;
							ebullets[eb].life = 3.5f;
							break;
						}
					}
				}
				sfx(260, 80, 0.18f, 160);
			}
		}

		e->x = clampf(-arena_hx + e->half_size, e->x, arena_hx - e->half_size);
		e->y = clampf(arena_min_y + e->half_size, e->y, arena_max_y - e->half_size);

		// Orbit blade collision
		if (orbit_blades > 0 && e->blade_cd <= 0.f) {
			for (int b = 0; b < orbit_blades; b++) {
				float a = orbit_angle + (float)b * (6.283185f / (float)orbit_blades);
				float bx = px + cosf(a) * 8.5f;
				float by = py + sinf(a) * 8.5f;
				if (absf(bx - e->x) < e->half_size + 1.2f && absf(by - e->y) < e->half_size + 1.2f) {
					e->blade_cd = 0.25f;
					damage_enemy(i, 2.2f, 0.f, 0.f, th);
					break;
				}
			}
		}

		if (e->active && p_invuln <= 0.f &&
		    absf(e->x - px) < e->half_size + 1.8f && absf(e->y - py) < e->half_size + 1.8f) {
			p_hp -= (e->kind == ENM_TANK || e->kind == ENM_BOSS) ? 18.f : 12.f;
			p_invuln = 0.65f;
			shake_timer = 0.16f; shake_mag = 0.9f;
			sfx(180, 110, 0.24f, 90);
		}
	}

	if (p_hp <= 0.f) {
		if (score > user_stats.survivor_high_score) user_stats.survivor_high_score = score;
		if (wave > user_stats.survivor_best_wave)   user_stats.survivor_best_wave = wave;
		save_user_data();
		game_state = STATE_GAME_OVER;
		return GAME_CONTINUE;
	}

	// Update XP gems
	for (int i = 0; i < SURV_MAX_GEMS; i++) {
		if (!gems[i].active) continue;
		float dx = px - gems[i].x;
		float dy = py - gems[i].y;
		float d2 = dx * dx + dy * dy;
		if (d2 < magnet_radius * magnet_radius && d2 > 0.01f) {
			float d = sqrtf(d2);
			gems[i].x += (dx / d) * 65.f * dt;
			gems[i].y += (dy / d) * 65.f * dt;
		}
		if (d2 < 9.f) {
			xp += gems[i].value;
			score += 10;
			gems[i].active = false;
			sfx(920, 20, 0.08f, 1180);
			if (xp >= xp_next) {
				xp -= xp_next;
				xp_next = (int)(xp_next * 1.35f) + 6;
				player_lvl++;
				roll_upgrade_choices();
				game_state = STATE_UPGRADE;
				sfx(523, 180, 0.24f, 1046);
				return GAME_CONTINUE;
			}
		}
	}

	draw_world(th);
	return GAME_CONTINUE;
}

} // namespace Survivor

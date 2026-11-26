#include <Windows.h>
#include <stdio.h>
#include "save.h"
#include "platform.h"
#include "keybinds.h"
#include "utils.h"

User_Stats user_stats = { 0, true, "PLAYER 1", 0, false, 0, 0, 0, 0 };

internal bool
get_save_path(char* out_path, int max_len, bool create_dir) {
	char appdata[260];
	DWORD len = GetEnvironmentVariableA("APPDATA", appdata, sizeof(appdata));
	if (len == 0 || len >= sizeof(appdata)) return false;

	char dir[260];
	snprintf(dir, sizeof(dir), "%s\\ArcadeHub", appdata);
	if (create_dir) CreateDirectoryA(dir, NULL);

	snprintf(out_path, (size_t)max_len, "%s\\save.cfg", dir);
	return true;
}

internal bool
str_eq(const char* a, const char* b) {
	int i = 0;
	while (a[i] && b[i]) {
		if (a[i] != b[i]) return false;
		i++;
	}
	return a[i] == b[i];
}

internal void
copy_str(char* dst, const char* src, int max_len) {
	int i = 0;
	while (src[i] && i < max_len - 1) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
}

internal int
parse_int(const char* s) {
	int sign = 1, val = 0, i = 0;
	if (s[0] == '-') { sign = -1; i = 1; }
	for (; s[i] >= '0' && s[i] <= '9'; i++) {
		val = val * 10 + (s[i] - '0');
	}
	return val * sign;
}

void
load_user_data() {
	char path[300];
	if (!get_save_path(path, sizeof(path), false)) return;

	FILE* f = fopen(path, "r");
	if (!f) return;

	char line[256];
	while (fgets(line, sizeof(line), f)) {
		int len = 0;
		while (line[len]) len++;
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
			line[--len] = 0;
		}
		int eq = -1;
		for (int i = 0; line[i]; i++) {
			if (line[i] == '=') { eq = i; break; }
		}
		if (eq <= 0) continue;

		line[eq] = 0;
		const char* k = line;
		const char* val_str = line + eq + 1;
		int v = parse_int(val_str);

		if (str_eq(k, "hub_theme"))              user_stats.hub_theme = clamp(0, v, 2);
		else if (str_eq(k, "hub_bgm_on"))        user_stats.hub_bgm_on = (v != 0);
		else if (str_eq(k, "player_name")) {
			if (val_str[0] != 0) copy_str(user_stats.player_name, val_str, sizeof(user_stats.player_name));
		}
		else if (str_eq(k, "avatar_id"))         user_stats.avatar_id = clamp(0, v, 5);
		else if (str_eq(k, "profile_created"))   user_stats.profile_created = (v != 0);
		else if (str_eq(k, "audio_muted"))       set_audio_muted(v != 0);
		else if (str_eq(k, "vol_master"))        set_master_volume(v);
		else if (str_eq(k, "vol_music"))         set_music_volume(v);
		else if (str_eq(k, "vol_sfx"))           set_sfx_volume(v);
		else if (str_eq(k, "pong_p1_up"))        pong_binds.p1_up = clamp(1, v, 255);
		else if (str_eq(k, "pong_p1_down"))      pong_binds.p1_down = clamp(1, v, 255);
		else if (str_eq(k, "pong_p2_up"))        pong_binds.p2_up = clamp(1, v, 255);
		else if (str_eq(k, "pong_p2_down"))      pong_binds.p2_down = clamp(1, v, 255);
		else if (str_eq(k, "pong_pause"))        pong_binds.pause = clamp(1, v, 255);
		else if (str_eq(k, "pong_mouse"))        pong_binds.mouse_1p = (v != 0);
		else if (str_eq(k, "brk_left"))          breaker_binds.left = clamp(1, v, 255);
		else if (str_eq(k, "brk_right"))         breaker_binds.right = clamp(1, v, 255);
		else if (str_eq(k, "brk_action"))        breaker_binds.action = clamp(1, v, 255);
		else if (str_eq(k, "brk_pause"))         breaker_binds.pause = clamp(1, v, 255);
		else if (str_eq(k, "brk_mouse"))         breaker_binds.mouse_aim = (v != 0);
		else if (str_eq(k, "pong_best_rally"))   user_stats.pong_best_rally = clamp(0, v, 99999);
		else if (str_eq(k, "pong_wins"))         user_stats.pong_wins = clamp(0, v, 99999);
		else if (str_eq(k, "brk_high_score"))    user_stats.breaker_high_score = clamp(0, v, 9999999);
		else if (str_eq(k, "brk_unlocked_lvl"))  user_stats.breaker_unlocked_level = clamp(0, v, 7);
	}
	fclose(f);
}

void
save_user_data() {
	char path[300];
	if (!get_save_path(path, sizeof(path), true)) return;

	FILE* f = fopen(path, "w");
	if (!f) return;

	fprintf(f, "hub_theme=%d\n", user_stats.hub_theme);
	fprintf(f, "hub_bgm_on=%d\n", user_stats.hub_bgm_on ? 1 : 0);
	fprintf(f, "player_name=%s\n", user_stats.player_name);
	fprintf(f, "avatar_id=%d\n", user_stats.avatar_id);
	fprintf(f, "profile_created=%d\n", user_stats.profile_created ? 1 : 0);
	fprintf(f, "audio_muted=%d\n", is_audio_muted() ? 1 : 0);
	fprintf(f, "vol_master=%d\n", get_master_volume());
	fprintf(f, "vol_music=%d\n", get_music_volume());
	fprintf(f, "vol_sfx=%d\n", get_sfx_volume());
	fprintf(f, "pong_p1_up=%d\n", pong_binds.p1_up);
	fprintf(f, "pong_p1_down=%d\n", pong_binds.p1_down);
	fprintf(f, "pong_p2_up=%d\n", pong_binds.p2_up);
	fprintf(f, "pong_p2_down=%d\n", pong_binds.p2_down);
	fprintf(f, "pong_pause=%d\n", pong_binds.pause);
	fprintf(f, "pong_mouse=%d\n", pong_binds.mouse_1p ? 1 : 0);
	fprintf(f, "brk_left=%d\n", breaker_binds.left);
	fprintf(f, "brk_right=%d\n", breaker_binds.right);
	fprintf(f, "brk_action=%d\n", breaker_binds.action);
	fprintf(f, "brk_pause=%d\n", breaker_binds.pause);
	fprintf(f, "brk_mouse=%d\n", breaker_binds.mouse_aim ? 1 : 0);
	fprintf(f, "pong_best_rally=%d\n", user_stats.pong_best_rally);
	fprintf(f, "pong_wins=%d\n", user_stats.pong_wins);
	fprintf(f, "brk_high_score=%d\n", user_stats.breaker_high_score);
	fprintf(f, "brk_unlocked_lvl=%d\n", user_stats.breaker_unlocked_level);

	fclose(f);
}

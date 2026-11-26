#include <stdio.h>
#include "catalog.h"
#include "platform.h"

internal void
copy_str(char* dst, const char* src, int max_len) {
	if (max_len <= 0) return;
	int i = 0;
	while (src[i] && i < max_len - 1) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
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
trim_line(char* s) {
	int start = 0;
	while (s[start] == ' ' || s[start] == '\t') start++;
	int len = 0;
	while (s[start + len]) len++;
	while (len > 0 && (s[start + len - 1] == '\n' || s[start + len - 1] == '\r' ||
	                   s[start + len - 1] == ' '  || s[start + len - 1] == '\t')) {
		len--;
	}
	for (int i = 0; i < len; i++) s[i] = s[start + i];
	s[len] = 0;
}

internal bool
is_absolute_path(const char* p) {
	if (!p || !p[0]) return false;
	if (p[0] == '\\' || p[0] == '/') return true;
	if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') return true;
	return false;
}

internal void
join_path(char* out, int max_len, const char* base, const char* rel) {
	if (is_absolute_path(rel)) {
		copy_str(out, rel, max_len);
		return;
	}
	int pos = 0;
	for (int i = 0; base[i] && pos < max_len - 2; i++) {
		out[pos++] = base[i];
	}
	if (pos > 0 && out[pos - 1] != '\\' && out[pos - 1] != '/') {
		out[pos++] = '\\';
	}
	for (int i = 0; rel[i] && pos < max_len - 1; i++) {
		out[pos++] = (rel[i] == '/') ? '\\' : rel[i];
	}
	out[pos] = 0;
}

internal u32
parse_hex_color(const char* s) {
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
	u32 val = 0;
	for (int i = 0; s[i]; i++) {
		char c = s[i];
		u32 digit = 0;
		if (c >= '0' && c <= '9') digit = (u32)(c - '0');
		else if (c >= 'a' && c <= 'f') digit = (u32)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F') digit = (u32)(c - 'A' + 10);
		else break;
		val = (val << 4) | digit;
	}
	return val;
}

void
refresh_catalog_availability(Game_Catalog* catalog) {
	for (int i = 0; i < catalog->count; i++) {
		catalog->entries[i].available = file_exists(catalog->entries[i].exe);
	}
}

void
load_catalog(Game_Catalog* catalog, const char* cfg_filename) {
	catalog->count = 0;

	char exe_dir[260];
	get_exe_dir(exe_dir, sizeof(exe_dir));

	char cfg_path[260];
	join_path(cfg_path, sizeof(cfg_path), exe_dir, cfg_filename);

	FILE* f = fopen(cfg_path, "r");
	if (!f) return;

	char line[512];
	int current_idx = -1;

	while (fgets(line, sizeof(line), f)) {
		trim_line(line);
		if (line[0] == 0 || line[0] == '#' || line[0] == ';') continue;

		if (line[0] == '[') {
			if (catalog->count >= MAX_GAMES) {
				current_idx = -1;
				continue;
			}
			current_idx = catalog->count++;
			Game_Entry* e = &catalog->entries[current_idx];
			*e = {};
			e->accent = 0x38BDF8;
			copy_str(e->players, "1P", sizeof(e->players));
			copy_str(e->icon, "GAME", sizeof(e->icon));

			int j = 0;
			for (int i = 1; line[i] && line[i] != ']' && j < (int)sizeof(e->id) - 1; i++) {
				e->id[j++] = line[i];
			}
			e->id[j] = 0;
			copy_str(e->title, e->id, sizeof(e->title));
			continue;
		}

		if (current_idx < 0) continue;

		int eq = -1;
		for (int i = 0; line[i]; i++) {
			if (line[i] == '=') { eq = i; break; }
		}
		if (eq <= 0) continue;

		line[eq] = 0;
		char* key = line;
		char* val = line + eq + 1;
		trim_line(key);
		trim_line(val);

		Game_Entry* e = &catalog->entries[current_idx];
		if (str_eq(key, "title"))         copy_str(e->title, val, sizeof(e->title));
		else if (str_eq(key, "subtitle")) copy_str(e->subtitle, val, sizeof(e->subtitle));
		else if (str_eq(key, "players"))  copy_str(e->players, val, sizeof(e->players));
		else if (str_eq(key, "exe"))      join_path(e->exe, sizeof(e->exe), exe_dir, val);
		else if (str_eq(key, "workdir"))  join_path(e->workdir, sizeof(e->workdir), exe_dir, val);
		else if (str_eq(key, "accent"))   e->accent = parse_hex_color(val);
		else if (str_eq(key, "icon"))     copy_str(e->icon, val, sizeof(e->icon));
	}

	fclose(f);
	refresh_catalog_availability(catalog);
}


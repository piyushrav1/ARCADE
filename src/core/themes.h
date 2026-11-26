#pragma once

#include "utils.h"
#include "save.h"

struct Theme {
	const char* name;
	u32 bg_outer;
	u32 arena_border;
	u32 arena_bg;
	u32 center_line;
	u32 p1;
	u32 p2;
	u32 ball;
	u32 text;
	u32 text_dim;
	u32 accent;
	u32 panel_bg;
	u32 panel_border;
	u32 card_bg;
	u32 card_selected;
};

global_variable const Theme THEMES[3] = {
	{ "CYBER",     0x0B1120, 0x334155, 0x0F172A, 0x1E293B, 0x38BDF8, 0xF59E0B, 0xF8FAFC, 0xF1F5F9, 0x64748B, 0x22D3EE, 0x0F172A, 0x334155, 0x162033, 0x1E293B },
	{ "CRT",       0x020802, 0x166534, 0x051405, 0x14532D, 0x4ADE80, 0x86EFAC, 0xDCFCE7, 0xBBF7D0, 0x22C55E, 0x4ADE80, 0x051405, 0x166534, 0x082008, 0x14532D },
	{ "SYNTHWAVE", 0x120624, 0x7E22CE, 0x1E0B36, 0x4C1D95, 0x06B6D4, 0xEC4899, 0xFDF4FF, 0xFAE8FF, 0xA855F7, 0xF472B6, 0x1E0B36, 0x7E22CE, 0x2A104B, 0x4C1D95 },
};

inline const Theme&
resolve_theme(int override_idx) {
	int idx = (override_idx >= 0 && override_idx < 3) ? override_idx : clamp(0, user_stats.hub_theme, 2);
	return THEMES[idx];
}

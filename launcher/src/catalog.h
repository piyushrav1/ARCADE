#pragma once

#include "utils.h"

#define MAX_GAMES 16

struct Game_Entry {
	char id[32];
	char title[48];
	char subtitle[96];
	char players[16];
	char exe[260];
	char workdir[260];
	u32 accent;
	char icon[16];
	bool available;
};

struct Game_Catalog {
	Game_Entry entries[MAX_GAMES];
	int count;
};

void load_catalog(Game_Catalog* catalog, const char* cfg_filename);
void refresh_catalog_availability(Game_Catalog* catalog);


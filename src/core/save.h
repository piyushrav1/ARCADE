#pragma once

struct User_Stats {
	int hub_theme;
	int pong_theme;     // -1 = Follow Hub, 0..2 = Custom
	int breaker_theme;  // -1 = Follow Hub, 0..2 = Custom
	int survivor_theme; // -1 = Follow Hub, 0..2 = Custom
	bool hub_bgm_on;
	char player_name[16];
	int avatar_id;
	bool profile_created;
	int pong_best_rally;
	int pong_wins;
	int breaker_high_score;
	int breaker_unlocked_level;
	int survivor_high_score;
	int survivor_best_wave;
};

extern User_Stats user_stats;

void load_user_data();
void save_user_data();

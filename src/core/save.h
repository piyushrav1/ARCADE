#pragma once

struct User_Stats {
	int hub_theme;
	bool hub_bgm_on;
	int pong_best_rally;
	int pong_wins;
	int breaker_high_score;
	int breaker_unlocked_level;
};

extern User_Stats user_stats;

void load_user_data();
void save_user_data();

#pragma once

struct Button_State {
	bool is_down;
	bool changed;
};

enum {
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LEFT,
	BUTTON_RIGHT,
	BUTTON_W,
	BUTTON_S,
	BUTTON_ENTER,
	BUTTON_SPACE,
	BUTTON_ESC,
	BUTTON_M,
	BUTTON_T,
	BUTTON_B,
	BUTTON_C,
	BUTTON_V,
	BUTTON_MOUSE_LEFT,

	BUTTON_COUNT,
};

struct Input {
	Button_State buttons[BUTTON_COUNT];
	float mouse_x; // world coordinates
	float mouse_y;
	bool mouse_moved;
};

// Platform services implemented in platform_win32.cpp
void play_tone(int freq_hz, int duration_ms, float volume = 0.25f, float freq_end_hz = 0.f);
void set_bgm_enabled(bool enabled);
void set_audio_muted(bool muted);
void request_quit();
bool launch_game(const char* exe_path, const char* work_dir);
bool file_exists(const char* path);
void get_exe_dir(char* out_buf, int buf_size);

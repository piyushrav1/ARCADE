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
	BUTTON_A,
	BUTTON_S,
	BUTTON_D,
	BUTTON_ENTER,
	BUTTON_SPACE,
	BUTTON_ESC,
	BUTTON_P,
	BUTTON_M,
	BUTTON_T,
	BUTTON_B,
	BUTTON_F1,
	BUTTON_MOUSE_LEFT,

	BUTTON_COUNT,
};

struct Input {
	Button_State buttons[BUTTON_COUNT];
	float mouse_x;
	float mouse_y;
	bool mouse_moved;
};

void play_tone(int freq_hz, int duration_ms, float volume = 0.25f, float freq_end_hz = 0.f);
void set_bgm_enabled(bool enabled);
void set_audio_muted(bool muted);
bool is_audio_muted();
void request_quit();

void hub_init();
void hub_update(Input* input, float dt, float fps);


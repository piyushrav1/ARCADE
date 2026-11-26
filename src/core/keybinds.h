#pragma once

#include <Windows.h>
#include "platform.h"

struct Pong_Binds {
	int p1_up;
	int p1_down;
	int p2_up;
	int p2_down;
	int pause;
	bool mouse_1p;
};

struct Breaker_Binds {
	int left;
	int right;
	int action;
	int pause;
	bool mouse_aim;
};

extern Pong_Binds pong_binds;
extern Breaker_Binds breaker_binds;

inline bool vk_down(const Input* in, int vk) {
	return (vk > 0 && vk < 256) ? in->keys_down[vk] : false;
}

inline bool vk_pressed(const Input* in, int vk) {
	return (vk > 0 && vk < 256) ? (in->keys_down[vk] && in->keys_changed[vk]) : false;
}

void reset_pong_binds();
void reset_breaker_binds();
const char* vk_name(int vk);

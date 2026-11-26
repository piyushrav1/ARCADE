#pragma once

#include <Windows.h>
#include "utils.h"

struct Render_State {
	int height, width;
	void* memory;
	BITMAPINFO bitmap_info;
};

extern Render_State render_state;
extern float render_offset_x;
extern float render_offset_y;

void clear_screen(u32 color);
void draw_rect(float x, float y, float half_size_x, float half_size_y, u32 color);
void draw_rect_alpha(float x, float y, float half_size_x, float half_size_y, u32 color, float alpha);
void draw_text_centered(const char* text, float cx, float cy, float pixel_size, u32 color);
void draw_text_left(const char* text, float left_x, float cy, float pixel_size, u32 color);
void draw_number(int num, float cx, float cy, float pixel_size, u32 color);


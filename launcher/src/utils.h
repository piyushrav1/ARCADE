#pragma once

typedef unsigned char u8;
typedef char s8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long long s64;
typedef unsigned long long u64;

#define internal static
#define global_variable static

inline int
clamp(int min, int val, int max) {
	if (val < min) return min;
	if (val > max) return max;
	return val;
}

inline float
clampf(float min, float val, float max) {
	if (val < min) return min;
	if (val > max) return max;
	return val;
}

inline float
absf(float v) {
	return v < 0.f ? -v : v;
}

inline u32
lerp_color(u32 a, u32 b, float t) {
	t = clampf(0.f, t, 1.f);
	u8 ar = (a >> 16) & 0xff, ag = (a >> 8) & 0xff, ab = a & 0xff;
	u8 br = (b >> 16) & 0xff, bg = (b >> 8) & 0xff, bb = b & 0xff;
	u8 r = (u8)(ar + (br - ar) * t);
	u8 g = (u8)(ag + (bg - ag) * t);
	u8 bl = (u8)(ab + (bb - ab) * t);
	return (r << 16) | (g << 8) | bl;
}


#include <Windows.h>
#include <mmsystem.h>
#include <math.h>
#include "utils.h"
#include "platform.h"
#include "renderer.h"

global_variable bool running = true;
global_variable bool window_lost_focus = false;
global_variable HWND main_window = 0;
Render_State render_state;

#define AUDIO_SAMPLE_RATE 22050
#define AUDIO_BUF_SAMPLES 1024
#define AUDIO_NUM_BUFS    3
#define MAX_SFX_VOICES    4

struct Sfx_Voice {
	bool active;
	float phase;
	float freq_start;
	float freq_end;
	float volume;
	int samples_left;
	int total_samples;
};

global_variable HWAVEOUT h_wave_out = 0;
global_variable WAVEHDR wave_hdrs[AUDIO_NUM_BUFS];
global_variable s16 wave_bufs[AUDIO_NUM_BUFS][AUDIO_BUF_SAMPLES];
global_variable Sfx_Voice sfx_voices[MAX_SFX_VOICES];
global_variable bool audio_muted = false;
global_variable bool bgm_enabled = true;

global_variable const float BGM_LEAD[32] = {
	523.25f, 0.f, 659.25f, 783.99f,  659.25f, 0.f, 523.25f, 0.f,
	587.33f, 0.f, 698.46f, 880.00f,  783.99f, 0.f, 659.25f, 0.f,
	523.25f, 0.f, 659.25f, 783.99f, 1046.50f, 0.f, 987.77f, 880.00f,
	783.99f, 659.25f, 587.33f, 0.f,  659.25f, 0.f, 523.25f, 0.f
};

global_variable const float BGM_BASS[32] = {
	130.81f, 130.81f, 261.63f, 130.81f,  130.81f, 130.81f, 196.00f, 130.81f,
	146.83f, 146.83f, 293.66f, 146.83f,  196.00f, 196.00f, 246.94f, 196.00f,
	130.81f, 130.81f, 261.63f, 130.81f,  174.61f, 174.61f, 349.23f, 174.61f,
	196.00f, 196.00f, 146.83f, 146.83f,  130.81f, 130.81f, 196.00f, 130.81f
};

global_variable int   bgm_step = 0;
global_variable int   bgm_sample_in_step = 0;
global_variable float bgm_lead_phase = 0.f;
global_variable float bgm_bass_phase = 0.f;
global_variable u32   audio_noise_rng = 98765u;

internal void
audio_init() {
	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = AUDIO_SAMPLE_RATE;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = 2;
	wfx.nAvgBytesPerSec = AUDIO_SAMPLE_RATE * 2;

	if (waveOutOpen(&h_wave_out, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
		h_wave_out = 0;
		return;
	}

	for (int i = 0; i < AUDIO_NUM_BUFS; i++) {
		wave_hdrs[i] = {};
		wave_hdrs[i].lpData = (LPSTR)wave_bufs[i];
		wave_hdrs[i].dwBufferLength = AUDIO_BUF_SAMPLES * sizeof(s16);
		waveOutPrepareHeader(h_wave_out, &wave_hdrs[i], sizeof(WAVEHDR));
		wave_hdrs[i].dwFlags |= WHDR_DONE;
	}
}

internal void
audio_fill_and_submit(WAVEHDR* hdr, s16* buf) {
	const int samples_per_step = AUDIO_SAMPLE_RATE / 9;
	for (int i = 0; i < AUDIO_BUF_SAMPLES; i++) {
		float mix = 0.f;

		if (!audio_muted) {
			if (bgm_enabled) {
				float step_t = (float)bgm_sample_in_step / (float)samples_per_step;
				float lf = BGM_LEAD[bgm_step];
				if (lf > 0.f) {
					bgm_lead_phase += lf / (float)AUDIO_SAMPLE_RATE;
					if (bgm_lead_phase >= 1.f) bgm_lead_phase -= 1.f;
					float pulse = (bgm_lead_phase < 0.25f) ? 1.f : -1.f;
					float env = 1.f - step_t * 0.7f;
					mix += pulse * env * 0.075f;
				}

				float bf = BGM_BASS[bgm_step];
				if (bf > 0.f) {
					bgm_bass_phase += bf / (float)AUDIO_SAMPLE_RATE;
					if (bgm_bass_phase >= 1.f) bgm_bass_phase -= 1.f;
					float tri = 1.f - 4.f * absf(bgm_bass_phase - 0.5f);
					float benv = 1.f - step_t * 0.45f;
					mix += tri * benv * 0.095f;
				}

				if ((bgm_step & 1) == 1 && step_t < 0.18f) {
					audio_noise_rng = audio_noise_rng * 1664525u + 1013904223u;
					float n = ((float)(audio_noise_rng & 0xFFFF) / 32768.f) - 1.f;
					mix += n * (1.f - step_t / 0.18f) * 0.03f;
				}

				bgm_sample_in_step++;
				if (bgm_sample_in_step >= samples_per_step) {
					bgm_sample_in_step = 0;
					bgm_step = (bgm_step + 1) & 31;
				}
			}

			for (int v = 0; v < MAX_SFX_VOICES; v++) {
				if (!sfx_voices[v].active) continue;
				Sfx_Voice* sv = &sfx_voices[v];
				float prog = 1.f - ((float)sv->samples_left / (float)sv->total_samples);
				float freq = sv->freq_start + (sv->freq_end - sv->freq_start) * prog;
				sv->phase += freq / (float)AUDIO_SAMPLE_RATE;
				if (sv->phase >= 1.f) sv->phase -= 1.f;
				float sq = (sv->phase < 0.5f) ? 1.f : -1.f;
				float env = 1.f - prog;
				mix += sq * env * sv->volume;
				sv->samples_left--;
				if (sv->samples_left <= 0) sv->active = false;
			}
		}

		mix = clampf(-0.95f, mix, 0.95f);
		buf[i] = (s16)(mix * 32767.f);
	}

	waveOutWrite(h_wave_out, hdr, sizeof(WAVEHDR));
}

internal void
audio_pump() {
	if (!h_wave_out) return;
	for (int i = 0; i < AUDIO_NUM_BUFS; i++) {
		if (wave_hdrs[i].dwFlags & WHDR_DONE) {
			wave_hdrs[i].dwFlags &= ~WHDR_DONE;
			audio_fill_and_submit(&wave_hdrs[i], wave_bufs[i]);
		}
	}
}

internal void
audio_shutdown() {
	if (!h_wave_out) return;
	waveOutReset(h_wave_out);
	for (int i = 0; i < AUDIO_NUM_BUFS; i++) {
		waveOutUnprepareHeader(h_wave_out, &wave_hdrs[i], sizeof(WAVEHDR));
	}
	waveOutClose(h_wave_out);
	h_wave_out = 0;
}

void
play_tone(int freq_hz, int duration_ms, float volume, float freq_end_hz) {
	if (audio_muted) return;
	int samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
	if (samples <= 0) return;

	int slot = 0;
	for (int i = 0; i < MAX_SFX_VOICES; i++) {
		if (!sfx_voices[i].active) { slot = i; break; }
	}
	sfx_voices[slot].active = true;
	sfx_voices[slot].phase = 0.f;
	sfx_voices[slot].freq_start = (float)freq_hz;
	sfx_voices[slot].freq_end = (freq_end_hz > 0.f) ? freq_end_hz : (float)freq_hz;
	sfx_voices[slot].volume = volume * 0.45f;
	sfx_voices[slot].total_samples = samples;
	sfx_voices[slot].samples_left = samples;
}

void set_bgm_enabled(bool enabled) { bgm_enabled = enabled; }
void set_audio_muted(bool muted)  { audio_muted = muted; }
bool is_audio_muted()             { return audio_muted; }
void request_quit()               { running = false; }

internal void
screen_to_world(int sx, int sy, float* wx, float* wy) {
	if (render_state.height <= 0) {
		*wx = 0.f; *wy = 0.f; return;
	}
	float scale = render_state.height * 0.01f;
	float flipped_y = (float)(render_state.height - sy);
	*wx = ((float)sx - render_state.width * 0.5f) / scale;
	*wy = (flipped_y - render_state.height * 0.5f) / scale;
}

LRESULT CALLBACK window_callback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;
	switch (uMsg) {
		case WM_CLOSE:
		case WM_DESTROY: {
			running = false;
		} break;

		case WM_KILLFOCUS: {
			window_lost_focus = true;
		} break;

		case WM_SIZE: {
			RECT rect;
			GetClientRect(hwnd, &rect);
			render_state.width = rect.right - rect.left;
			render_state.height = rect.bottom - rect.top;

			int size = render_state.width * render_state.height * (int)sizeof(unsigned int);

			if (render_state.memory) VirtualFree(render_state.memory, 0, MEM_RELEASE);
			if (size > 0) {
				render_state.memory = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			} else {
				render_state.memory = 0;
			}

			render_state.bitmap_info.bmiHeader.biSize = sizeof(render_state.bitmap_info.bmiHeader);
			render_state.bitmap_info.bmiHeader.biWidth = render_state.width;
			render_state.bitmap_info.bmiHeader.biHeight = render_state.height;
			render_state.bitmap_info.bmiHeader.biPlanes = 1;
			render_state.bitmap_info.bmiHeader.biBitCount = 32;
			render_state.bitmap_info.bmiHeader.biCompression = BI_RGB;
		} break;

		default: {
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}
	return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	WNDCLASSA window_class = {};
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpszClassName = "Arcade Hub Window Class";
	window_class.lpfnWndProc = window_callback;
	window_class.hInstance = hInstance;

	RegisterClassA(&window_class);

	int win_w = 1280, win_h = 720;
	int win_x = (GetSystemMetrics(SM_CXSCREEN) - win_w) / 2;
	int win_y = (GetSystemMetrics(SM_CYSCREEN) - win_h) / 2;

	main_window = CreateWindowA(
		window_class.lpszClassName,
		"Arcade Hub",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		win_x, win_y, win_w, win_h,
		0, 0, hInstance, 0
	);
	ShowWindow(main_window, SW_SHOW);
	UpdateWindow(main_window);
	HDC hdc = GetDC(main_window);

	audio_init();
	hub_init();

	Input input = {};
	float delta_time = 0.016666f;
	float fps_smoothed = 60.f;
	LARGE_INTEGER frame_begin_time;
	QueryPerformanceCounter(&frame_begin_time);

	float performance_frequency;
	{
		LARGE_INTEGER perf;
		QueryPerformanceFrequency(&perf);
		performance_frequency = (float)perf.QuadPart;
	}

	while (running) {
		MSG message;

		for (int i = 0; i < BUTTON_COUNT; i++) {
			input.buttons[i].changed = false;
		}
		if (window_lost_focus) {
			window_lost_focus = false;
			for (int i = 0; i < BUTTON_COUNT; i++) {
				input.buttons[i].is_down = false;
			}
		}
		input.mouse_moved = false;

		while (PeekMessage(&message, main_window, 0, 0, PM_REMOVE)) {
			switch (message.message) {
				case WM_MOUSEMOVE: {
					int sx = (int)(short)LOWORD(message.lParam);
					int sy = (int)(short)HIWORD(message.lParam);
					screen_to_world(sx, sy, &input.mouse_x, &input.mouse_y);
					input.mouse_moved = true;
				} break;

				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP: {
					int sx = (int)(short)LOWORD(message.lParam);
					int sy = (int)(short)HIWORD(message.lParam);
					screen_to_world(sx, sy, &input.mouse_x, &input.mouse_y);
					bool down = (message.message == WM_LBUTTONDOWN);
					input.buttons[BUTTON_MOUSE_LEFT].changed = (down != input.buttons[BUTTON_MOUSE_LEFT].is_down);
					input.buttons[BUTTON_MOUSE_LEFT].is_down = down;
				} break;

				case WM_KEYUP:
				case WM_KEYDOWN: {
					u32 vk_code = (u32)message.wParam;
					bool is_down = ((message.lParam & (1 << 31)) == 0);

#define process_button(b, vk) \
	case vk: { \
		input.buttons[b].changed = is_down != input.buttons[b].is_down; \
		input.buttons[b].is_down = is_down; \
	} break;

					switch (vk_code) {
						process_button(BUTTON_UP, VK_UP);
						process_button(BUTTON_DOWN, VK_DOWN);
						process_button(BUTTON_LEFT, VK_LEFT);
						process_button(BUTTON_RIGHT, VK_RIGHT);
						process_button(BUTTON_W, 'W');
						process_button(BUTTON_A, 'A');
						process_button(BUTTON_S, 'S');
						process_button(BUTTON_D, 'D');
						process_button(BUTTON_ENTER, VK_RETURN);
						process_button(BUTTON_SPACE, VK_SPACE);
						process_button(BUTTON_ESC, VK_ESCAPE);
						process_button(BUTTON_P, 'P');
						process_button(BUTTON_M, 'M');
						process_button(BUTTON_T, 'T');
						process_button(BUTTON_B, 'B');
						process_button(BUTTON_F1, VK_F1);
					}
				} break;

				default: {
					TranslateMessage(&message);
					DispatchMessage(&message);
				}
			}
		}

		audio_pump();
		hub_update(&input, delta_time, fps_smoothed);

		if (render_state.memory) {
			StretchDIBits(
				hdc, 0, 0, render_state.width, render_state.height,
				0, 0, render_state.width, render_state.height,
				render_state.memory, &render_state.bitmap_info,
				DIB_RGB_COLORS, SRCCOPY
			);
		}

		LARGE_INTEGER frame_end_time;
		QueryPerformanceCounter(&frame_end_time);
		float raw_dt = (float)(frame_end_time.QuadPart - frame_begin_time.QuadPart) / performance_frequency;
		if (raw_dt > 0.0001f) {
			fps_smoothed = fps_smoothed * 0.9f + (1.f / raw_dt) * 0.1f;
		}
		delta_time = clampf(0.0001f, raw_dt, 0.05f);
		frame_begin_time = frame_end_time;
	}

	audio_shutdown();
	return 0;
}


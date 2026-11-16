/* Host preview for the AIR PS3 visual UI.
 *
 * Renders the same ui_app code the PS3 build uses, driven by a synthetic
 * song, and writes raw RGB24 frames to stdout (pipe into ffmpeg) or a single
 * PPM still. Also reports per-mode render cost.
 *
 *   ui_preview video > frames.rgb      (1280x720 rgb24, 30 fps)
 *   ui_preview still MODE THEME STATE out.ppm
 *   ui_preview bench
 */
#include "ui_app.h"
#include "ui_audio_tap.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RATE 48000.0f
#define PI_F 3.14159265358979f

static uint32_t out_px[UI_CANVAS_WIDTH * UI_CANVAS_HEIGHT];
static uint32_t bg_px[UI_CANVAS_WIDTH * UI_CANVAS_HEIGHT];
static uint32_t bg_hud_px[UI_CANVAS_WIDTH * UI_CANVAS_HEIGHT];
static uint32_t fx_px[UI_CANVAS_WIDTH * UI_CANVAS_HEIGHT];
static ui_app app;
static ui_audio_tap tap;
static uint32_t noise_state = 12345;

static float noise(void)
{
	noise_state = noise_state * 1664525U + 1013904223U;
	return (float)(noise_state >> 8) / 8388608.0f - 1.0f;
}

/* A small synthetic track: kick, bass, pad chords, hats. 118 BPM. */
static void synth(double t0, float *buf, size_t frames)
{
	static const float roots[4] = { 55.0f, 43.65f, 49.0f, 41.2f };
	size_t i;
	for (i = 0; i < frames; ++i) {
		double t = t0 + (double)i / RATE;
		double beat = t * 118.0 / 60.0;
		double bt = fmod(beat, 1.0) * 60.0 / 118.0;
		int bar = (int)(beat / 4.0) % 4;
		float root = roots[bar];
		float kick = (float)(sin(2.0 * PI_F * (45.0 * bt + 90.0 * (1.0 - exp(-bt * 30.0)) / 30.0)) *
			exp(-bt * 9.0));
		float bass = 0.32f * (float)sin(2.0 * PI_F * root * t) *
			(float)(0.6 + 0.4 * exp(-fmod(beat * 2.0, 1.0) * 4.0));
		float pad = 0.0f, hat, l, r;
		int n;
		static const float chord[3] = { 4.0f, 5.04f, 6.0f };
		for (n = 0; n < 3; ++n)
			pad += 0.06f * (float)sin(2.0 * PI_F * root * chord[n] * t +
				0.3 * sin(2.0 * PI_F * 0.4 * t));
		hat = fmod(beat * 2.0, 1.0) > 0.5 ?
			0.10f * noise() * (float)exp(-fmod(beat * 2.0 - 0.5, 1.0) * 25.0) : 0.0f;
		l = 0.55f * kick + bass + pad + hat * 0.8f;
		r = 0.55f * kick + bass + pad * 0.9f + hat;
		buf[i * 2] = l * 0.8f;
		buf[i * 2 + 1] = r * 0.8f;
	}
}

static void base_status(ui_status *st, int state)
{
	memset(st, 0, sizeof(*st));
	st->state = state;
	st->volume_percent = 72;
	(void)snprintf(st->receiver_name, sizeof(st->receiver_name), "AirPS3");
	(void)snprintf(st->address, sizeof(st->address), "192.168.1.42");
	if (state != UI_STATE_IDLE) {
		(void)snprintf(st->title, sizeof(st->title), "Midnight Drive");
		(void)snprintf(st->artist, sizeof(st->artist), "Nova Lights");
		(void)snprintf(st->album, sizeof(st->album), "Neon Coast");
		st->has_progress = 1;
		st->position_s = 42.0f;
		st->duration_s = 231.0f;
	}
}

static double song_time;
static float left[UI_FFT_SIZE], right[UI_FFT_SIZE];

static void step(float dt, uint64_t now_us, int audible)
{
	float block[800 * 2];
	size_t frames = (size_t)(dt * RATE);
	if (frames > 800) frames = 800;
	if (audible)
		synth(song_time, block, frames);
	else
		memset(block, 0, sizeof(block));
	song_time += (double)frames / RATE;
	ui_audio_tap_push(&tap, block, frames, 2);
	(void)ui_audio_tap_snapshot(&tap, left, right, UI_FFT_SIZE, 0);
	ui_app_render(&app, left, right, dt, now_us);
}

static void write_rgb(FILE *f)
{
	static unsigned char row[UI_CANVAS_WIDTH * 3];
	int x, y;
	for (y = 0; y < UI_CANVAS_HEIGHT; ++y) {
		for (x = 0; x < UI_CANVAS_WIDTH; ++x) {
			uint32_t p = out_px[y * UI_CANVAS_WIDTH + x];
			row[x * 3] = (unsigned char)(p >> 16);
			row[x * 3 + 1] = (unsigned char)(p >> 8);
			row[x * 3 + 2] = (unsigned char)p;
		}
		(void)fwrite(row, 1, sizeof(row), f);
	}
}

static void setup(void)
{
	ui_app_init(&app, out_px, bg_px, bg_hud_px, fx_px);
	ui_audio_tap_init(&tap);
}

static int video(void)
{
	const float dt = 1.0f / 60.0f;
	ui_status st;
	int frame, total = 60 * 38;
	setup();
	base_status(&st, UI_STATE_IDLE);
	ui_app_set_status(&app, &st);
	for (frame = 0; frame < total; ++frame) {
		float t = (float)frame * dt;
		uint64_t now = (uint64_t)(t * 1e6f);
		int audible = t >= 5.0f;
		if (frame == 60 * 4) { base_status(&st, UI_STATE_CONNECTING); st.title[0] = 0; ui_app_set_status(&app, &st); }
		if (frame == 60 * 5) { base_status(&st, UI_STATE_PLAYING); st.progress_stamp_us = now; ui_app_set_status(&app, &st); }
		if (frame == 60 * 10) ui_app_command(&app, UI_CMD_NEXT_MODE);
		if (frame == 60 * 15) ui_app_command(&app, UI_CMD_NEXT_MODE);
		if (frame == 60 * 17) { ui_app_command(&app, UI_CMD_NEXT_THEME); }
		if (frame == 60 * 20) ui_app_command(&app, UI_CMD_NEXT_MODE);
		if (frame == 60 * 22) { st.volume_percent = 45; st.volume_updates++; ui_app_set_status(&app, &st); }
		if (frame == 60 * 25) { ui_app_command(&app, UI_CMD_NEXT_MODE); ui_app_command(&app, UI_CMD_NEXT_THEME); }
		if (frame == 60 * 30) { ui_app_command(&app, UI_CMD_NEXT_MODE); ui_app_command(&app, UI_CMD_NEXT_THEME); }
		if (frame == 60 * 34) ui_app_command(&app, UI_CMD_TOGGLE_HUD);
		step(dt, now, audible);
		if (frame % 2 == 0)
			write_rgb(stdout);
	}
	return 0;
}

static int still(int mode, int theme, int state, const char *path)
{
	const float dt = 1.0f / 60.0f;
	ui_status st;
	FILE *f;
	int frame;
	setup();
	ui_app_apply_settings(&app, mode, theme, 1);
	base_status(&st, state);
	ui_app_set_status(&app, &st);
	app.hint_timer = 100.0f;
	for (frame = 0; frame < 60 * 6 + 17; ++frame)
		step(dt, (uint64_t)frame * 16667U, state == UI_STATE_PLAYING);
	f = fopen(path, "wb");
	if (f == NULL)
		return 1;
	fprintf(f, "P6\n%d %d\n255\n", UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT);
	write_rgb(f);
	return fclose(f) != 0;
}

static int bench(void)
{
	int mode;
	for (mode = 0; mode < UI_VIS_COUNT; ++mode) {
		ui_status st;
		clock_t start;
		int frame, frames = 300;
		double ms;
		setup();
		ui_app_apply_settings(&app, mode, 0, 1);
		base_status(&st, UI_STATE_PLAYING);
		ui_app_set_status(&app, &st);
		start = clock();
		for (frame = 0; frame < frames; ++frame)
			step(1.0f / 60.0f, (uint64_t)frame * 16667U, 1);
		ms = (double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC / frames;
		printf("%-9s %.2f ms/frame (host)\n", ui_visualizer_name(mode), ms);
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 2 && strcmp(argv[1], "video") == 0)
		return video();
	if (argc >= 6 && strcmp(argv[1], "still") == 0)
		return still(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), argv[5]);
	if (argc >= 2 && strcmp(argv[1], "bench") == 0)
		return bench();
	fprintf(stderr, "usage: %s video | still MODE THEME STATE out.ppm | bench\n", argv[0]);
	return 2;
}

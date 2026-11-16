#include "visual_renderer.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lv2/systime.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <sys/thread.h>

#define CANVAS_PIXELS ((size_t)UI_CANVAS_WIDTH * (size_t)UI_CANVAS_HEIGHT)
#define FRAME_INTERVAL_US (1000000U / VISUAL_TARGET_FPS)
#define MENU_REFRESH_US 16667U
#ifndef FLIP_TIMEOUT_US
#define FLIP_TIMEOUT_US 500000U
#endif
#ifndef SETTINGS_SAVE_DELAY_US
#define SETTINGS_SAVE_DELAY_US 3000000U
#endif
/* Below the audio/network main thread, above the log writer (3071). */
#define RENDER_THREAD_PRIORITY 2000
#define RENDER_THREAD_STACK (64U * 1024U)

static ui_app app;
static float wave_left[UI_FFT_SIZE];
static float wave_right[UI_FFT_SIZE];

static void copy_canvas(visual_renderer *r, uint32_t buffer_index)
{
	diagnostic_screen *s = r->screen;
	uint32_t *dst = (uint32_t *)s->buffers[buffer_index];
	const uint32_t *src = app.out.px;
	uint32_t y, x;
	if (s->width == UI_CANVAS_WIDTH && s->height == UI_CANVAS_HEIGHT) {
		for (y = 0; y < s->height; ++y)
			memcpy((uint8_t *)dst + (size_t)y * s->pitch,
				src + (size_t)y * UI_CANVAS_WIDTH,
				UI_CANVAS_WIDTH * sizeof(uint32_t));
		return;
	}
	for (y = 0; y < s->height; ++y) {
		uint32_t *row = (uint32_t *)((uint8_t *)dst + (size_t)y * s->pitch);
		const uint32_t *src_row = src + (size_t)r->y_map[y] * UI_CANVAS_WIDTH;
		for (x = 0; x < s->width; ++x)
			row[x] = src_row[r->x_map[x]];
	}
}

static int issue_flip(visual_renderer *r, uint32_t buffer, uint64_t now)
{
	diagnostic_screen *s = r->screen;
	gcmContextData *context = (gcmContextData *)s->context;
	gcmResetFlipStatus();
	if (gcmSetFlip(context, buffer) != 0)
		return 0;
	rsxFlushBuffer(context);
	/* GPU-side wait; the CPU never blocks on the flip. */
	gcmSetWaitFlip(context);
	s->next_buffer = buffer ^ 1U;
	s->flip_started_us = now;
	s->flip_pending = 1;
	return 1;
}

static int read_status(visual_renderer *r, ui_status *out)
{
	int attempt;
	for (attempt = 0; attempt < 4; ++attempt) {
		uint32_t before = r->status_sequence;
		if (before & 1U) {
			usleep(50);
			continue;
		}
		__sync_synchronize();
		memcpy(out, &r->status, sizeof(*out));
		__sync_synchronize();
		if (r->status_sequence == before)
			return 1;
	}
	return 0;
}

static void drain_commands(visual_renderer *r)
{
	while (r->command_read != r->command_write) {
		uint32_t index = r->command_read % VISUAL_COMMAND_QUEUE;
		ui_command command = (ui_command)r->commands[index];
		__sync_synchronize();
		r->command_read = r->command_read + 1U;
		ui_app_command(&app, command);
	}
}

static void render_thread(void *argument)
{
	visual_renderer *r = (visual_renderer *)argument;
	diagnostic_screen *s = r->screen;
	uint64_t last_frame_us = sysGetSystemTime();
	uint64_t last_flip_us = 0;
	uint64_t dirty_since_us = 0;
	uint32_t seen_sequence = 0xffffffffU;
	ui_status status;

	r->running = 1;
	while (!r->stop_requested) {
		uint64_t now = sysGetSystemTime();
		int menu = r->system_menu_open;
		uint64_t render_started, copy_started;
		uint32_t elapsed;

		/* Never draw into a buffer that may still be on screen. */
		if (s->flip_pending) {
			if (gcmGetFlipStatus() != 0) {
				if (!menu && now - s->flip_started_us > FLIP_TIMEOUT_US) {
					r->error = VISUAL_RENDERER_TIMEOUT;
					break;
				}
				usleep(500);
				continue;
			}
			s->flip_pending = 0;
		}

		if (menu) {
			/* The XMB overlay composites over our scanout: keep feeding it
			 * paced flips of the two finished frames, but do not redraw. */
			if (now - last_flip_us >= MENU_REFRESH_US) {
				if (!issue_flip(r, s->next_buffer, now)) {
					r->error = VISUAL_RENDERER_FLIP_FAILED;
					break;
				}
				last_flip_us = now;
			} else {
				usleep(1000);
			}
			last_frame_us = now; /* no time jump when the menu closes */
			continue;
		}

		if (now - last_flip_us + 1000U < FRAME_INTERVAL_US) {
			usleep(1000);
			continue;
		}

		drain_commands(r);
		if (r->status_sequence != seen_sequence && read_status(r, &status)) {
			seen_sequence = r->status_sequence;
			ui_app_set_status(&app, &status);
		}

		render_started = sysGetSystemTime();
		(void)ui_audio_tap_snapshot(r->tap, wave_left, wave_right, UI_FFT_SIZE,
			VISUAL_AUDIO_DELAY_FRAMES);
		ui_app_render(&app, wave_left, wave_right,
			(float)(render_started - last_frame_us) / 1000000.0f, render_started);
		last_frame_us = render_started;
		copy_started = sysGetSystemTime();
		elapsed = (uint32_t)(copy_started - render_started);
		if (elapsed > r->render_us_max)
			r->render_us_max = elapsed;

		copy_canvas(r, s->next_buffer);
		now = sysGetSystemTime();
		elapsed = (uint32_t)(now - copy_started);
		if (elapsed > r->copy_us_max)
			r->copy_us_max = elapsed;

		if (!issue_flip(r, s->next_buffer, now)) {
			r->error = VISUAL_RENDERER_FLIP_FAILED;
			break;
		}
		last_flip_us = now;
		r->frames = r->frames + 1U;

		/* Persist mode/theme/info choices a few seconds after the last change. */
		if (app.settings_dirty) {
			if (dirty_since_us == 0)
				dirty_since_us = now;
			if (now - dirty_since_us > SETTINGS_SAVE_DELAY_US) {
				if (ui_settings_save(r->settings_path, app.mode, app.theme,
					app.hud_visible))
					r->settings_saves = r->settings_saves + 1U;
				app.settings_dirty = 0;
				dirty_since_us = 0;
			}
		}
	}

	if (app.settings_dirty)
		(void)ui_settings_save(r->settings_path, app.mode, app.theme,
			app.hud_visible);
	/* Leave the scanout with no flip in flight so diagnostic_screen can take
	 * over again (or close) safely. */
	{
		uint64_t deadline = sysGetSystemTime() + FLIP_TIMEOUT_US;
		while (s->flip_pending && gcmGetFlipStatus() != 0 &&
			(uint64_t)sysGetSystemTime() < deadline)
			usleep(500);
		if (gcmGetFlipStatus() == 0)
			s->flip_pending = 0;
	}
	__sync_synchronize();
	r->running = 0;
	sysThreadExit(0);
}

int visual_renderer_start(visual_renderer *r, diagnostic_screen *screen,
	const ui_audio_tap *tap, const char *settings_path)
{
	sys_ppu_thread_t thread;
	int mode = UI_VIS_SPECTRUM, theme = 0, hud = 1;
	uint32_t i;
	if (r == NULL || screen == NULL || !screen->initialized || tap == NULL ||
		settings_path == NULL)
		return VISUAL_RENDERER_BAD_ARGUMENT;
	memset(r, 0, sizeof(*r));
	r->screen = screen;
	r->tap = tap;
	(void)snprintf(r->settings_path, sizeof(r->settings_path), "%s", settings_path);

	r->pixels = (uint32_t *)memalign(128, CANVAS_PIXELS * 4U * sizeof(uint32_t));
	r->x_map = (uint32_t *)malloc((size_t)screen->width * sizeof(uint32_t));
	r->y_map = (uint32_t *)malloc((size_t)screen->height * sizeof(uint32_t));
	if (r->pixels == NULL || r->x_map == NULL || r->y_map == NULL) {
		free(r->pixels);
		free(r->x_map);
		free(r->y_map);
		memset(r, 0, sizeof(*r));
		return VISUAL_RENDERER_MEMORY_FAILED;
	}
	for (i = 0; i < screen->width; ++i)
		r->x_map[i] = (i * UI_CANVAS_WIDTH) / screen->width;
	for (i = 0; i < screen->height; ++i)
		r->y_map[i] = (i * UI_CANVAS_HEIGHT) / screen->height;

	ui_app_init(&app, r->pixels, r->pixels + CANVAS_PIXELS,
		r->pixels + CANVAS_PIXELS * 2U, r->pixels + CANVAS_PIXELS * 3U);
	r->settings_loaded = ui_settings_load(r->settings_path, &mode, &theme, &hud);
	ui_app_apply_settings(&app, mode, theme, hud);

	/* diagnostic_screen may still have a flip in flight; the thread waits for it. */
	r->running = 1;
	if (sysThreadCreate(&thread, render_thread, r, RENDER_THREAD_PRIORITY,
		RENDER_THREAD_STACK, THREAD_JOINABLE, "air-ps3-visual") != 0) {
		r->running = 0;
		free(r->pixels);
		free(r->x_map);
		free(r->y_map);
		memset(r, 0, sizeof(*r));
		return VISUAL_RENDERER_THREAD_FAILED;
	}
	r->thread_id = (uint64_t)thread;
	r->thread_started = 1;
	return VISUAL_RENDERER_OK;
}

int visual_renderer_stop(visual_renderer *r)
{
	int error;
	if (r == NULL || !r->thread_started)
		return 0;
	r->stop_requested = 1;
	__sync_synchronize();
	(void)sysThreadJoin((sys_ppu_thread_t)r->thread_id, NULL);
	error = r->error;
	free(r->pixels);
	free(r->x_map);
	free(r->y_map);
	r->pixels = NULL;
	r->x_map = r->y_map = NULL;
	r->thread_started = 0;
	return error;
}

void visual_renderer_set_menu(visual_renderer *r, int open)
{
	if (r != NULL)
		r->system_menu_open = open;
}

void visual_renderer_command(visual_renderer *r, ui_command command)
{
	uint32_t write;
	if (r == NULL || !r->thread_started)
		return;
	write = r->command_write;
	if (write - r->command_read >= VISUAL_COMMAND_QUEUE)
		return; /* drop rather than block the audio thread */
	r->commands[write % VISUAL_COMMAND_QUEUE] = (uint8_t)command;
	__sync_synchronize();
	r->command_write = write + 1U;
}

void visual_renderer_publish(visual_renderer *r, const ui_status *status)
{
	if (r == NULL || status == NULL)
		return;
	r->status_sequence = r->status_sequence + 1U;
	__sync_synchronize();
	memcpy(&r->status, status, sizeof(*status));
	__sync_synchronize();
	r->status_sequence = r->status_sequence + 1U;
}

void visual_renderer_take_stats(visual_renderer *r, uint32_t *frames,
	uint32_t *render_us_max, uint32_t *copy_us_max)
{
	*frames = r->frames;
	*render_us_max = r->render_us_max;
	*copy_us_max = r->copy_us_max;
	r->render_us_max = 0;
	r->copy_us_max = 0;
}

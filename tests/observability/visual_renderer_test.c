/* Runs the production visual_renderer thread against mocked RSX/flip/thread
 * APIs: frames render and flip, the XMB menu pauses drawing but keeps flips
 * paced, commands reach the UI, settings persist, and a stuck flip stops the
 * renderer so the main thread can fall back to the static screen. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "visual_renderer.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <lv2/systime.h>
#include <rsx/rsx.h>
#include <sys/thread.h>

static volatile int flips;
static volatile int flip_stuck;
static volatile int last_flip_buffer = -1;

uint64_t sysGetSystemTime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000U + (uint64_t)ts.tv_nsec / 1000U;
}
int gcmGetFlipStatus(void) { return flip_stuck; }
void gcmResetFlipStatus(void) {}
int gcmSetFlip(gcmContextData *c, uint32_t buffer)
{
	(void)c;
	assert(buffer < 2);
	last_flip_buffer = (int)buffer;
	__sync_fetch_and_add(&flips, 1);
	return 0;
}
void gcmSetWaitFlip(gcmContextData *c) { (void)c; }
void rsxFlushBuffer(gcmContextData *c) { (void)c; }

typedef struct { void (*entry)(void *); void *arg; } start_info;
static void *trampoline(void *p)
{
	start_info info = *(start_info *)p;
	free(p);
	info.entry(info.arg);
	return NULL;
}
int sysThreadCreate(sys_ppu_thread_t *id, void (*entry)(void *), void *arg,
	int priority, uint64_t stack, uint64_t flags, const char *name)
{
	pthread_t thread;
	start_info *info = malloc(sizeof(*info));
	(void)priority; (void)stack; (void)flags; (void)name;
	info->entry = entry;
	info->arg = arg;
	if (pthread_create(&thread, NULL, trampoline, info) != 0)
		return -1;
	*id = (sys_ppu_thread_t)thread;
	return 0;
}
int sysThreadJoin(sys_ppu_thread_t id, uint64_t *retval)
{
	(void)retval;
	return pthread_join((pthread_t)id, NULL);
}
void sysThreadExit(uint64_t value) { (void)value; pthread_exit(NULL); }

static void wait_for(volatile uint32_t *counter, uint32_t target)
{
	int i;
	for (i = 0; i < 400 && *counter < target; ++i)
		usleep(10000);
	assert(*counter >= target);
}

static void run_at(uint16_t width, uint16_t height)
{
	static visual_renderer r;
	static ui_audio_tap tap;
	diagnostic_screen screen;
	const char *settings = "build/forensic/visual-settings.txt";
	uint32_t pitch = (uint32_t)width * 4U;
	uint32_t frames_before, render_us, copy_us, frames;
	int flips_before;
	FILE *f;
	char text[64];
	size_t n;

	memset(&screen, 0, sizeof(screen));
	screen.initialized = 1;
	screen.width = width;
	screen.height = height;
	screen.pitch = pitch;
	screen.next_buffer = 1;
	screen.buffers[0] = calloc((size_t)pitch * height, 1);
	screen.buffers[1] = calloc((size_t)pitch * height, 1);
	(void)remove(settings);
	ui_audio_tap_init(&tap);
	flip_stuck = 0;

	assert(visual_renderer_start(&r, &screen, &tap, settings) == VISUAL_RENDERER_OK);
	assert(r.settings_loaded == 0);
	wait_for(&r.frames, 5);
	/* the drawn frame reached the framebuffer (background is never black) */
	assert(((uint32_t *)screen.buffers[0])[(height / 2) * (pitch / 4) + 5] != 0 ||
		((uint32_t *)screen.buffers[1])[(height / 2) * (pitch / 4) + 5] != 0);

	{
		ui_status st;
		memset(&st, 0, sizeof(st));
		st.state = UI_STATE_PLAYING;
		(void)snprintf(st.title, sizeof(st.title), "Test");
		visual_renderer_publish(&r, &st);
	}

	/* Menu open: flips keep coming, frames stop. */
	visual_renderer_set_menu(&r, 1);
	usleep(60000);
	frames_before = r.frames;
	flips_before = flips;
	usleep(150000);
	assert(r.frames == frames_before);
	assert(flips - flips_before >= 4);
	visual_renderer_set_menu(&r, 0);
	wait_for(&r.frames, frames_before + 3);

	/* Commands apply and persist after the debounce. */
	visual_renderer_command(&r, UI_CMD_NEXT_MODE);
	visual_renderer_command(&r, UI_CMD_NEXT_MODE);
	visual_renderer_command(&r, UI_CMD_NEXT_THEME);
	visual_renderer_command(&r, UI_CMD_TOGGLE_HUD);
	wait_for(&r.settings_saves, 1);
	f = fopen(settings, "rb");
	assert(f != NULL);
	n = fread(text, 1, sizeof(text) - 1, f);
	fclose(f);
	text[n] = '\0';
	assert(strcmp(text, "mode=2\ntheme=1\nhud=0\n") == 0);

	visual_renderer_take_stats(&r, &frames, &render_us, &copy_us);
	assert(frames > 0 && render_us > 0);
	assert(visual_renderer_stop(&r) == 0);
	assert(!r.running);

	/* A restarted renderer picks the saved settings back up. */
	assert(visual_renderer_start(&r, &screen, &tap, settings) == VISUAL_RENDERER_OK);
	assert(r.settings_loaded == 1);
	wait_for(&r.frames, 2);

	/* A flip that never completes stops the renderer with a timeout. */
	flip_stuck = 1;
	{
		int i;
		for (i = 0; i < 300 && r.running; ++i)
			usleep(10000);
	}
	assert(!r.running);
	assert(visual_renderer_stop(&r) == VISUAL_RENDERER_TIMEOUT);
	flip_stuck = 0;
	(void)remove(settings);
	free(screen.buffers[0]);
	free(screen.buffers[1]);
}

int main(void)
{
	run_at(1280, 720);
	run_at(1920, 1080);
	run_at(720, 480);
	puts("visual renderer: paced frames, menu pause, commands, settings, flip timeout PASS");
	return 0;
}

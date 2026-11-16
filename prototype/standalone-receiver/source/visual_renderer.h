#ifndef PS3_AIRPLAY_VISUAL_RENDERER_H
#define PS3_AIRPLAY_VISUAL_RENDERER_H

#include <stdint.h>

#include "diagnostic_screen.h"
#include "ui_app.h"
#include "ui_audio_tap.h"

/* Animated UI on a dedicated low-priority PPU thread.
 *
 * The renderer borrows the double-buffered scanout that diagnostic_screen
 * already set up (and which shows the static artwork while it starts). While
 * it runs, it is the only code that touches RSX; the main thread talks to it
 * through the small lock-free fields below and never blocks on it. If it stops
 * with an error, the main thread can go back to diagnostic_screen_present().
 *
 * The render thread never logs: receiver_log is single-producer, so the main
 * thread reads the counters here and logs them. */

#ifndef VISUAL_TARGET_FPS
#define VISUAL_TARGET_FPS 60
#endif
#ifndef VISUAL_AUDIO_DELAY_FRAMES
#define VISUAL_AUDIO_DELAY_FRAMES 1024 /* ~21 ms at 48 kHz: lines visuals up with sound */
#endif
#define VISUAL_COMMAND_QUEUE 16

enum visual_renderer_result {
	VISUAL_RENDERER_OK = 0,
	VISUAL_RENDERER_BAD_ARGUMENT = -1,
	VISUAL_RENDERER_MEMORY_FAILED = -2,
	VISUAL_RENDERER_THREAD_FAILED = -3,
	VISUAL_RENDERER_FLIP_FAILED = -4,
	VISUAL_RENDERER_TIMEOUT = -5
};

typedef struct {
	diagnostic_screen *screen;
	const ui_audio_tap *tap;
	char settings_path[128];
	uint64_t thread_id;
	uint32_t *pixels; /* one allocation for the four UI canvases */
	uint32_t *x_map;  /* canvas column for each scanout column */
	uint32_t *y_map;
	int thread_started;

	/* main -> render */
	volatile int stop_requested;
	volatile int system_menu_open;
	volatile uint32_t status_sequence; /* odd while being written */
	ui_status status;
	volatile uint32_t command_write;
	volatile uint32_t command_read;
	volatile uint8_t commands[VISUAL_COMMAND_QUEUE];

	/* render -> main */
	volatile int running;
	volatile int error;
	volatile uint32_t frames;
	volatile uint32_t render_us_max;
	volatile uint32_t copy_us_max;
	volatile uint32_t settings_saves;
	volatile int settings_loaded;
} visual_renderer;

int visual_renderer_start(visual_renderer *renderer, diagnostic_screen *screen,
	const ui_audio_tap *tap, const char *settings_path);
/* Stops and joins the thread. Returns 0 or a visual_renderer_result. */
int visual_renderer_stop(visual_renderer *renderer);

void visual_renderer_set_menu(visual_renderer *renderer, int open);
void visual_renderer_command(visual_renderer *renderer, ui_command command);
void visual_renderer_publish(visual_renderer *renderer, const ui_status *status);
/* Reads and resets the per-interval maxima. */
void visual_renderer_take_stats(visual_renderer *renderer, uint32_t *frames,
	uint32_t *render_us_max, uint32_t *copy_us_max);

#endif

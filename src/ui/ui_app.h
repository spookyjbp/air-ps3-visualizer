#ifndef PS3_AIRPLAY_UI_APP_H
#define PS3_AIRPLAY_UI_APP_H

#include <stdint.h>

#include "ui_canvas.h"
#include "ui_spectrum.h"
#include "ui_visualizer.h"

#define UI_CANVAS_WIDTH 1280
#define UI_CANVAS_HEIGHT 720
#define UI_TEXT_MAX 128

typedef enum {
	UI_STATE_IDLE = 0,       /* no sender connected */
	UI_STATE_CONNECTING = 1, /* RTSP session set up, not streaming yet */
	UI_STATE_PLAYING = 2,
	UI_STATE_PAUSED = 3
} ui_playback_state;

/* Snapshot of receiver state published by the audio/network thread. */
typedef struct {
	int state;
	char title[UI_TEXT_MAX];
	char artist[UI_TEXT_MAX];
	char album[UI_TEXT_MAX];
	int has_progress;
	float position_s;        /* playback position at progress_stamp_us */
	float duration_s;
	uint64_t progress_stamp_us;
	int volume_percent;      /* 0..100 */
	int muted;
	uint32_t volume_updates; /* changes trigger the volume indicator */
	char receiver_name[32];
	char address[24];
} ui_status;

typedef enum {
	UI_CMD_NEXT_MODE = 0,
	UI_CMD_PREV_MODE,
	UI_CMD_NEXT_THEME,
	UI_CMD_PREV_THEME,
	UI_CMD_TOGGLE_HUD,
	UI_CMD_SHOW_HINTS
} ui_command;

typedef struct {
	ui_canvas out; /* composed frame */
	ui_canvas bg;  /* cached theme background */
	ui_canvas bg_hud; /* same, with the legibility scrim behind the info text */
	ui_canvas fx;  /* additive light layer (keeps trails between frames) */
	ui_spectrum spectrum;
	ui_visualizer vis;
	ui_status status;

	int mode;
	int theme;
	int hud_visible;
	int settings_dirty;

	int bg_theme;
	int drawn_mode;
	float time;
	float play_blend;
	float hud_blend;
	float hint_timer;
	float toast_timer;
	char toast[48];
	char toast_detail[64];
	float volume_timer;
	uint32_t seen_volume_updates;
	int seen_volume_initialized;
	float shown_volume;
} ui_app;

/* Buffers must each hold UI_CANVAS_WIDTH * UI_CANVAS_HEIGHT pixels. */
void ui_app_init(ui_app *app, uint32_t *out, uint32_t *bg, uint32_t *bg_hud,
	uint32_t *fx);
void ui_app_apply_settings(ui_app *app, int mode, int theme, int hud_visible);
void ui_app_command(ui_app *app, ui_command command);
void ui_app_set_status(ui_app *app, const ui_status *status);
/* left/right hold UI_FFT_SIZE recent output frames. */
void ui_app_render(ui_app *app, const float *left, const float *right,
	float dt, uint64_t now_us);

/* Settings file: "mode=N\ntheme=N\nhud=N\n". Missing/invalid keys keep the
 * values already in *mode etc. Returns 1 if the file was read. */
int ui_settings_load(const char *path, int *mode, int *theme, int *hud_visible);
int ui_settings_save(const char *path, int mode, int theme, int hud_visible);
int ui_settings_parse(const char *text, int *mode, int *theme, int *hud_visible);

#endif

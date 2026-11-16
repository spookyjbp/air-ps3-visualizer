#include "ui_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MARGIN_X 80
#define TOP_Y 70
#define TITLE_Y 566
#define BODY_Y 608
#define PROGRESS_Y 636
#define FOOT_Y 668
#define RIGHT_COLUMN 400
#define HINT_SECONDS 7.0f
#define TOAST_SECONDS 1.9f
#define VOLUME_SECONDS 2.4f

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static float approach(float value, float target, float rate, float dt)
{
	return value + (target - value) * (1.0f - expf(-dt * rate));
}

static void bind(ui_canvas *c, uint32_t *px)
{
	c->px = px;
	c->w = UI_CANVAS_WIDTH;
	c->h = UI_CANVAS_HEIGHT;
	c->stride = UI_CANVAS_WIDTH;
}

void ui_app_init(ui_app *app, uint32_t *out, uint32_t *bg, uint32_t *bg_hud,
	uint32_t *fx)
{
	memset(app, 0, sizeof(*app));
	bind(&app->out, out);
	bind(&app->bg, bg);
	bind(&app->bg_hud, bg_hud);
	bind(&app->fx, fx);
	ui_canvas_fill(&app->fx, 0);
	ui_spectrum_init(&app->spectrum, 48000.0f);
	ui_visualizer_init(&app->vis);
	app->mode = UI_VIS_SPECTRUM;
	app->theme = 0;
	app->hud_visible = 1;
	app->hud_blend = 1.0f;
	app->bg_theme = -1;
	app->drawn_mode = -1;
	app->hint_timer = HINT_SECONDS + 3.0f; /* show controls a bit longer at launch */
	app->status.volume_percent = 100;
}

void ui_app_apply_settings(ui_app *app, int mode, int theme, int hud_visible)
{
	if (mode >= 0 && mode < UI_VIS_COUNT)
		app->mode = mode;
	if (theme >= 0 && theme < UI_THEME_COUNT)
		app->theme = theme;
	app->hud_visible = hud_visible ? 1 : 0;
	app->hud_blend = (float)app->hud_visible;
}

static void show_toast(ui_app *app, const char *title, const char *detail)
{
	(void)snprintf(app->toast, sizeof(app->toast), "%s", title);
	(void)snprintf(app->toast_detail, sizeof(app->toast_detail), "%s", detail);
	app->toast_timer = TOAST_SECONDS;
}

void ui_app_command(ui_app *app, ui_command command)
{
	char detail[64];
	app->hint_timer = HINT_SECONDS;
	switch (command) {
	case UI_CMD_NEXT_MODE:
	case UI_CMD_PREV_MODE:
		app->mode = (app->mode + (command == UI_CMD_NEXT_MODE ? 1 : UI_VIS_COUNT - 1))
			% UI_VIS_COUNT;
		(void)snprintf(detail, sizeof(detail), "Visualizer %d of %d%s",
			app->mode + 1, UI_VIS_COUNT,
			app->status.state == UI_STATE_PLAYING ? "" : " \xc2\xb7 starts with music");
		show_toast(app, ui_visualizer_name(app->mode), detail);
		app->settings_dirty = 1;
		break;
	case UI_CMD_NEXT_THEME:
	case UI_CMD_PREV_THEME:
		app->theme = (app->theme + (command == UI_CMD_NEXT_THEME ? 1 : UI_THEME_COUNT - 1))
			% UI_THEME_COUNT;
		(void)snprintf(detail, sizeof(detail), "Theme %d of %d", app->theme + 1,
			UI_THEME_COUNT);
		show_toast(app, ui_theme_get(app->theme)->name, detail);
		app->settings_dirty = 1;
		break;
	case UI_CMD_TOGGLE_HUD:
		app->hud_visible = !app->hud_visible;
		show_toast(app, app->hud_visible ? "Info shown" : "Info hidden",
			"Press Square to toggle");
		app->settings_dirty = 1;
		break;
	case UI_CMD_SHOW_HINTS:
	default:
		break;
	}
}

void ui_app_set_status(ui_app *app, const ui_status *status)
{
	app->status = *status;
	if (!app->seen_volume_initialized) {
		app->seen_volume_updates = status->volume_updates;
		app->seen_volume_initialized = 1;
		app->shown_volume = (float)status->volume_percent;
	} else if (status->volume_updates != app->seen_volume_updates) {
		app->seen_volume_updates = status->volume_updates;
		app->volume_timer = VOLUME_SECONDS;
	}
}

/* ---------- HUD pieces ---------- */

static void format_time(char *out, size_t size, float seconds)
{
	int s = (int)(seconds + 0.5f);
	if (s < 0) s = 0;
	if (s >= 3600)
		(void)snprintf(out, size, "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
	else
		(void)snprintf(out, size, "%d:%02d", s / 60, s % 60);
}

static int fade_alpha(float base, float blend)
{
	return (int)clampf(base * blend, 0.0f, 255.0f);
}

static void draw_brand(ui_app *app, const ui_theme *t, float a)
{
	ui_canvas *c = &app->out;
	float pulse = app->status.state == UI_STATE_PLAYING ?
		0.6f + 0.4f * app->spectrum.beat : 0.6f;
	ui_circle_blend(c, (float)MARGIN_X + 5.0f, (float)TOP_Y - 7.0f, 5.0f,
		t->accent_a, fade_alpha(255.0f * pulse, a));
	(void)ui_text_draw(c, UI_FONT_SMALL, MARGIN_X + 20, TOP_Y, "AIR PS3",
		t->text, fade_alpha(215.0f, a), 10);
}

static void draw_status_pill(ui_app *app, const ui_theme *t, float a)
{
	ui_canvas *c = &app->out;
	const char *label;
	uint32_t dot;
	int w, x, h = 32;
	switch (app->status.state) {
	case UI_STATE_PLAYING: label = "PLAYING"; dot = t->accent_a; break;
	case UI_STATE_PAUSED: label = "PAUSED"; dot = 0xffc35a; break;
	case UI_STATE_CONNECTING: label = "CONNECTING"; dot = t->accent_b; break;
	default: label = "READY"; dot = 0x8a93a6; break;
	}
	w = ui_text_width(UI_FONT_SMALL, label, 8) + 50;
	x = UI_CANVAS_WIDTH - MARGIN_X - w;
	ui_round_rect_blend(c, (float)x, (float)(TOP_Y - 23), (float)w, (float)h,
		16.0f, t->text, fade_alpha(22.0f, a));
	ui_circle_blend(c, (float)x + 20.0f, (float)TOP_Y - 7.0f, 4.5f, dot,
		fade_alpha(255.0f, a));
	(void)ui_text_draw(c, UI_FONT_SMALL, x + 34, TOP_Y, label, t->text,
		fade_alpha(220.0f, a), 8);
}

static void draw_now_playing(ui_app *app, const ui_theme *t, float a,
	uint64_t now_us)
{
	ui_canvas *c = &app->out;
	const ui_status *st = &app->status;
	char title[UI_TEXT_MAX + 4], body[UI_TEXT_MAX * 2 + 8], line[UI_TEXT_MAX * 2 + 8];
	int max_w = UI_CANVAS_WIDTH - MARGIN_X * 2 - RIGHT_COLUMN;
	if (st->state == UI_STATE_IDLE) {
		ui_text_fit(UI_FONT_TITLE, "Ready for AirPlay", 0, max_w, title, sizeof(title));
		(void)snprintf(line, sizeof(line), "Choose %s in your iPhone's AirPlay menu",
			st->receiver_name[0] ? st->receiver_name : "AirPS3");
	} else {
		ui_text_fit(UI_FONT_TITLE, st->title[0] ? st->title :
			(st->state == UI_STATE_CONNECTING ? "Connecting\xe2\x80\xa6" : "AirPlay audio"),
			0, max_w, title, sizeof(title));
		if (st->artist[0] && st->album[0])
			(void)snprintf(line, sizeof(line), "%s \xc2\xb7 %s", st->artist, st->album);
		else if (st->artist[0] || st->album[0])
			(void)snprintf(line, sizeof(line), "%s", st->artist[0] ? st->artist : st->album);
		else
			(void)snprintf(line, sizeof(line), "Streaming to %s",
				st->receiver_name[0] ? st->receiver_name : "AirPS3");
	}
	ui_text_fit(UI_FONT_BODY, line, 0, max_w, body, sizeof(body));
	(void)ui_text_draw(c, UI_FONT_TITLE, MARGIN_X, TITLE_Y, title, t->text,
		fade_alpha(255.0f, a), -2);
	(void)ui_text_draw(c, UI_FONT_BODY, MARGIN_X, BODY_Y, body, t->text,
		fade_alpha(165.0f, a), 0);

	if (st->state != UI_STATE_IDLE && st->has_progress && st->duration_s > 1.0f) {
		float pos = st->position_s;
		float bar_w = 520.0f, frac;
		char left[16], right[16];
		int rw;
		if (st->state == UI_STATE_PLAYING && now_us > st->progress_stamp_us)
			pos += (float)(now_us - st->progress_stamp_us) / 1000000.0f;
		pos = clampf(pos, 0.0f, st->duration_s);
		frac = pos / st->duration_s;
		ui_round_rect_blend(c, (float)MARGIN_X, (float)PROGRESS_Y, bar_w, 4.0f,
			2.0f, t->text, fade_alpha(40.0f, a));
		if (frac > 0.0f)
			ui_round_rect_blend(c, (float)MARGIN_X, (float)PROGRESS_Y,
				bar_w * frac < 4.0f ? 4.0f : bar_w * frac, 4.0f, 2.0f,
				t->accent_a, fade_alpha(235.0f, a));
		format_time(left, sizeof(left), pos);
		format_time(right, sizeof(right), st->duration_s);
		(void)ui_text_draw(c, UI_FONT_SMALL, MARGIN_X, FOOT_Y, left, t->text,
			fade_alpha(150.0f, a), 2);
		rw = ui_text_width(UI_FONT_SMALL, right, 2);
		(void)ui_text_draw(c, UI_FONT_SMALL, MARGIN_X + (int)bar_w - rw, FOOT_Y,
			right, t->text, fade_alpha(150.0f, a), 2);
	} else if (st->state == UI_STATE_IDLE && st->address[0]) {
		(void)snprintf(line, sizeof(line), "%s  \xc2\xb7  AirPlay 1 receiver", st->address);
		(void)ui_text_draw(c, UI_FONT_SMALL, MARGIN_X, FOOT_Y - 12, line, t->text,
			fade_alpha(110.0f, a), 4);
	}
}

typedef enum { ICON_LEFT_RIGHT, ICON_TRIANGLE, ICON_SQUARE, ICON_START } hint_icon;

static int draw_icon(ui_canvas *c, hint_icon icon, int x, int y, uint32_t color,
	int alpha, int measure_only)
{
	float cy = (float)y - 7.0f;
	if (icon == ICON_LEFT_RIGHT) {
		if (!measure_only) {
			float lx[3] = { (float)x, (float)x + 9.0f, (float)x + 9.0f };
			float ly[3] = { cy, cy - 6.0f, cy + 6.0f };
			float rx[3] = { (float)x + 24.0f, (float)x + 15.0f, (float)x + 15.0f };
			ui_triangle_blend(c, lx, ly, color, alpha);
			ui_triangle_blend(c, rx, ly, color, alpha);
		}
		return 24;
	}
	if (icon == ICON_TRIANGLE) {
		if (!measure_only) {
			float x0 = (float)x + 1.0f, x1 = (float)x + 8.0f, x2 = (float)x + 15.0f;
			ui_line_add(c, x0, cy + 6.0f, x1, cy - 7.0f, 2.0f, color, alpha);
			ui_line_add(c, x1, cy - 7.0f, x2, cy + 6.0f, 2.0f, color, alpha);
			ui_line_add(c, x2, cy + 6.0f, x0, cy + 6.0f, 2.0f, color, alpha);
		}
		return 16;
	}
	if (icon == ICON_SQUARE) {
		if (!measure_only) {
			float x0 = (float)x + 1.0f, x1 = (float)x + 14.0f;
			float y0 = cy - 6.5f, y1 = cy + 6.5f;
			ui_line_add(c, x0, y0, x1, y0, 2.0f, color, alpha);
			ui_line_add(c, x1, y0, x1, y1, 2.0f, color, alpha);
			ui_line_add(c, x1, y1, x0, y1, 2.0f, color, alpha);
			ui_line_add(c, x0, y1, x0, y0, 2.0f, color, alpha);
		}
		return 15;
	}
	/* START: a small capsule label */
	{
		int w = ui_text_width(UI_FONT_SMALL, "START", 2) + 16;
		if (!measure_only) {
			ui_round_rect_blend(c, (float)x, cy - 12.0f, (float)w, 24.0f, 12.0f,
				color, alpha / 5);
			(void)ui_text_draw(c, UI_FONT_SMALL, x + 8, y, "START", color, alpha, 2);
		}
		return w;
	}
}

static void draw_hints(ui_app *app, const ui_theme *t, float a)
{
	static const hint_icon icons[4] = { ICON_LEFT_RIGHT, ICON_TRIANGLE, ICON_SQUARE, ICON_START };
	static const char *const labels[4] = { "Visualizer", "Theme", "Info", "Exit" };
	ui_canvas *c = &app->out;
	int alpha = fade_alpha(185.0f, a);
	int total = 0, i, x;
	if (alpha <= 2)
		return;
	for (i = 0; i < 4; ++i)
		total += draw_icon(c, icons[i], 0, 0, 0, 0, 1) + 9 +
			ui_text_width(UI_FONT_SMALL, labels[i], 2) + (i < 3 ? 30 : 0);
	x = UI_CANVAS_WIDTH - MARGIN_X - total;
	for (i = 0; i < 4; ++i) {
		x += draw_icon(c, icons[i], x, FOOT_Y, t->text, alpha, 0) + 9;
		x = ui_text_draw(c, UI_FONT_SMALL, x, FOOT_Y, labels[i], t->text, alpha, 2) + 30;
	}
}

static void draw_mode_indicator(ui_app *app, const ui_theme *t, float a)
{
	ui_canvas *c = &app->out;
	char label[64];
	int i, w, x;
	const int dot_gap = 8;
	(void)snprintf(label, sizeof(label), "%s  \xc2\xb7  %s",
		ui_visualizer_name(app->mode), t->name);
	w = ui_text_width(UI_FONT_SMALL, label, 6);
	(void)ui_text_draw(c, UI_FONT_SMALL, UI_CANVAS_WIDTH - MARGIN_X - w, BODY_Y,
		label, t->text, fade_alpha(150.0f, a), 6);
	/* dots: current one is a short pill */
	x = UI_CANVAS_WIDTH - MARGIN_X - ((UI_VIS_COUNT - 1) * (6 + dot_gap) + 20);
	for (i = 0; i < UI_VIS_COUNT; ++i) {
		int current = i == app->mode;
		int dw = current ? 20 : 6;
		ui_round_rect_blend(c, (float)x, (float)PROGRESS_Y - 1.0f, (float)dw, 6.0f,
			3.0f, current ? t->accent_a : t->text,
			fade_alpha(current ? 240.0f : 70.0f, a));
		x += dw + dot_gap;
	}
}

static void draw_toast(ui_app *app, const ui_theme *t)
{
	ui_canvas *c = &app->out;
	float life = TOAST_SECONDS - app->toast_timer;
	float a = app->toast_timer <= 0.0f ? 0.0f :
		clampf(life / 0.15f, 0.0f, 1.0f) * clampf(app->toast_timer / 0.45f, 0.0f, 1.0f);
	int tw, dw, w, x, y = 112;
	float rise = (1.0f - clampf(life / 0.25f, 0.0f, 1.0f)) * 10.0f;
	if (a <= 0.01f)
		return;
	tw = ui_text_width(UI_FONT_BODY, app->toast, 0);
	dw = ui_text_width(UI_FONT_SMALL, app->toast_detail, 2);
	w = (tw > dw ? tw : dw) + 72;
	x = (UI_CANVAS_WIDTH - w) / 2;
	ui_round_rect_blend(c, (float)x, (float)y + rise, (float)w, 88.0f, 22.0f,
		0x000000, fade_alpha(120.0f, a));
	ui_round_rect_blend(c, (float)x, (float)y + rise, (float)w, 88.0f, 22.0f,
		t->text, fade_alpha(16.0f, a));
	(void)ui_text_draw(c, UI_FONT_BODY, (UI_CANVAS_WIDTH - tw) / 2,
		y + 42 + (int)rise, app->toast, t->text, fade_alpha(255.0f, a), 0);
	(void)ui_text_draw(c, UI_FONT_SMALL, (UI_CANVAS_WIDTH - dw) / 2,
		y + 70 + (int)rise, app->toast_detail, t->text, fade_alpha(150.0f, a), 2);
}

static void draw_volume(ui_app *app, const ui_theme *t)
{
	ui_canvas *c = &app->out;
	float a = clampf(app->volume_timer / 0.4f, 0.0f, 1.0f);
	float vol = app->status.muted ? 0.0f : app->shown_volume;
	float bar_w = 180.0f;
	int x = UI_CANVAS_WIDTH - MARGIN_X - 268, y = TOP_Y + 34;
	char label[16];
	int lw;
	if (a <= 0.01f)
		return;
	ui_round_rect_blend(c, (float)x, (float)y, 268.0f, 44.0f, 22.0f, 0x000000,
		fade_alpha(110.0f, a));
	{
		/* speaker glyph */
		float sx[3] = { (float)x + 30.0f, (float)x + 30.0f, (float)x + 20.0f };
		float sy[3] = { (float)y + 12.0f, (float)y + 32.0f, (float)y + 22.0f };
		ui_rect_blend(c, x + 16, y + 17, 8, 10, t->text, fade_alpha(230.0f, a));
		ui_triangle_blend(c, sx, sy, t->text, fade_alpha(230.0f, a));
	}
	ui_round_rect_blend(c, (float)x + 44.0f, (float)y + 20.0f, bar_w - 40.0f, 4.0f,
		2.0f, t->text, fade_alpha(45.0f, a));
	if (vol > 0.5f)
		ui_round_rect_blend(c, (float)x + 44.0f, (float)y + 20.0f,
			(bar_w - 40.0f) * vol / 100.0f, 4.0f, 2.0f, t->accent_a,
			fade_alpha(255.0f, a));
	(void)snprintf(label, sizeof(label), app->status.muted ? "Muted" : "%d%%",
		(int)(vol + 0.5f));
	lw = ui_text_width(UI_FONT_SMALL, label, 2);
	(void)ui_text_draw(c, UI_FONT_SMALL, x + 268 - 20 - lw, y + 29, label, t->text,
		fade_alpha(220.0f, a), 2);
}

/* ---------- frame ---------- */

/* Darken behind the info text so it stays legible over bright visuals. In
 * the steady state this is baked into bg_hud; it is applied live only while
 * the info panel fades in or out. */
static void apply_scrim(ui_canvas *c, const ui_theme *theme, float amount)
{
	int y;
	for (y = TITLE_Y - 80; y < UI_CANVAS_HEIGHT; ++y) {
		float f = clampf((float)(y - (TITLE_Y - 80)) / 120.0f, 0.0f, 1.0f);
		ui_rect_blend(c, 0, y, UI_CANVAS_WIDTH, 1, theme->bg_bottom,
			(int)(f * 150.0f * amount));
	}
}

void ui_app_render(ui_app *app, const float *left, const float *right,
	float dt, uint64_t now_us)
{
	const ui_theme *theme = ui_theme_get(app->theme);
	int playing = app->status.state == UI_STATE_PLAYING ||
		app->status.state == UI_STATE_PAUSED;
	int mode = playing ? app->mode : UI_VIS_RIBBONS;
	ui_vis_layout layout;
	float hud, hint_a;

	if (dt <= 0.0f || dt > 0.25f)
		dt = 1.0f / 60.0f;
	app->time += dt;
	app->toast_timer = clampf(app->toast_timer - dt, 0.0f, 100.0f);
	app->volume_timer = clampf(app->volume_timer - dt, 0.0f, 100.0f);
	app->hint_timer = clampf(app->hint_timer - dt, 0.0f, 100.0f);
	app->play_blend = approach(app->play_blend, playing ? 1.0f : 0.0f, 2.5f, dt);
	app->hud_blend = approach(app->hud_blend, (float)app->hud_visible, 7.0f, dt);
	app->shown_volume = approach(app->shown_volume,
		(float)app->status.volume_percent, 14.0f, dt);

	if (app->bg_theme != app->theme) {
		ui_canvas_background(&app->bg, theme->bg_top, theme->bg_bottom,
			theme->bg_glow, 0.5f, 0.42f, 0.55f);
		ui_canvas_copy(&app->bg_hud, &app->bg);
		apply_scrim(&app->bg_hud, theme, 1.0f);
		app->bg_theme = app->theme;
	}

	ui_spectrum_update(&app->spectrum, left, right, dt);

	if (mode != app->drawn_mode) {
		ui_canvas_fill(&app->fx, 0);
		app->drawn_mode = mode;
	} else {
		ui_canvas_fade(&app->fx, ui_visualizer_trail_keep(mode));
	}

	hud = app->hud_blend;
	layout.center_x = UI_CANVAS_WIDTH * 0.5f;
	layout.center_y = 312.0f + (360.0f - 312.0f) * (1.0f - hud);
	layout.width = 1100.0f;
	layout.height = 400.0f + 120.0f * (1.0f - hud);
	layout.intensity = 0.45f + 0.55f * app->play_blend;
	if (app->status.state == UI_STATE_PAUSED)
		layout.intensity *= 0.7f;
	ui_visualizer_draw(&app->vis, &app->fx, mode, theme, &app->spectrum,
		&layout, app->time, dt);

	if (hud > 0.995f) {
		ui_canvas_add(&app->out, &app->bg_hud, &app->fx);
	} else {
		ui_canvas_add(&app->out, &app->bg, &app->fx);
		if (hud > 0.01f)
			apply_scrim(&app->out, theme, hud);
	}

	if (hud > 0.01f) {
		draw_brand(app, theme, hud);
		draw_status_pill(app, theme, hud);
		draw_now_playing(app, theme, hud, now_us);
		draw_mode_indicator(app, theme, hud);
	}
	hint_a = clampf(app->hint_timer / 0.6f, 0.0f, 1.0f) * hud;
	draw_hints(app, theme, hint_a);
	draw_volume(app, theme);
	draw_toast(app, theme);
}

/* ---------- settings ---------- */

int ui_settings_parse(const char *text, int *mode, int *theme, int *hud_visible)
{
	const char *p = text;
	if (text == NULL)
		return 0;
	while (*p) {
		char key[16];
		int value, consumed = 0;
		if (sscanf(p, " %15[a-z] = %d%n", key, &value, &consumed) == 2 && consumed > 0) {
			if (strcmp(key, "mode") == 0 && value >= 0 && value < UI_VIS_COUNT)
				*mode = value;
			else if (strcmp(key, "theme") == 0 && value >= 0 && value < UI_THEME_COUNT)
				*theme = value;
			else if (strcmp(key, "hud") == 0 && (value == 0 || value == 1))
				*hud_visible = value;
		}
		while (*p && *p != '\n')
			++p;
		if (*p == '\n')
			++p;
	}
	return 1;
}

int ui_settings_load(const char *path, int *mode, int *theme, int *hud_visible)
{
	char text[256];
	size_t n;
	FILE *file = fopen(path, "rb");
	if (file == NULL)
		return 0;
	n = fread(text, 1, sizeof(text) - 1, file);
	(void)fclose(file);
	text[n] = '\0';
	return ui_settings_parse(text, mode, theme, hud_visible);
}

int ui_settings_save(const char *path, int mode, int theme, int hud_visible)
{
	FILE *file = fopen(path, "wb");
	int ok;
	if (file == NULL)
		return 0;
	ok = fprintf(file, "mode=%d\ntheme=%d\nhud=%d\n", mode, theme,
		hud_visible ? 1 : 0) > 0;
	if (fclose(file) != 0)
		ok = 0;
	return ok;
}

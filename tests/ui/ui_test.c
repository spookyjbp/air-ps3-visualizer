#include "../../src/ui/ui_app.h"
#include "../../src/ui/ui_audio_tap.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_pixels(void)
{
	assert(ui_rgb_add(0xf0f0f0, 0x202020) == 0xffffff);
	assert(ui_rgb_add(0x102030, 0x010203) == 0x112233);
	assert(ui_rgb_add(0xff0000, 0x0000ff) == 0xff00ff);
	assert(ui_rgb_add(0x00ff00, 0x00ff00) == 0x00ff00);
	assert(ui_rgb_scale(0xffffff, 256) == 0xffffff);
	assert(ui_rgb_scale(0xffffff, 0) == 0);
	assert(ui_rgb_blend(0x000000, 0xffffff, 256) == 0xffffff);
	assert(ui_rgb_blend(0x123456, 0xffffff, 0) == 0x123456);
	{
		uint32_t mid = ui_rgb_blend(0x000000, 0xfefefe, 128);
		assert(((mid >> 16) & 0xff) == 0x7f && (mid & 0xff) == 0x7f);
	}
}

static void test_text(void)
{
	const char *s = "A\xc3\xa9\xe2\x80\x99\xf0\x9f\x8e\xb5";
	char out[64];
	assert(ui_utf8_next(&s) == 'A');
	assert(ui_utf8_next(&s) == 0xe9);
	assert(ui_utf8_next(&s) == 0x2019);
	assert(ui_utf8_next(&s) == 0x1f3b5);
	assert(*s == '\0');
	assert(ui_font_glyph_index(0x2019) == '\'' - 32);
	assert(ui_font_glyph_index(0x1f3b5) == '?' - 32);
	assert(ui_font_glyph_index(0xe9) == 95 + 0xe9 - 160);
	assert(ui_text_width(UI_FONT_BODY, "", 0) == 0);
	assert(ui_text_width(UI_FONT_BODY, "WWW", 0) > ui_text_width(UI_FONT_BODY, "iii", 0));
	ui_text_fit(UI_FONT_BODY, "Short", 0, 1000, out, sizeof(out));
	assert(strcmp(out, "Short") == 0);
	ui_text_fit(UI_FONT_BODY, "A very long song title that cannot fit", 0, 200, out,
		sizeof(out));
	assert(ui_text_width(UI_FONT_BODY, out, 0) <= 200);
	assert(strlen(out) > 3 && strcmp(out + strlen(out) - 3, "...") == 0);
	/* never cuts inside a multi-byte sequence */
	ui_text_fit(UI_FONT_BODY, "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9", 0,
		60, out, sizeof(out));
	{
		const char *p = out;
		while (*p)
			assert(ui_utf8_next(&p) != 0xfffd);
	}
	ui_text_fit(UI_FONT_BODY, "Anything", 0, 2, out, sizeof(out));
	assert(out[0] == '\0');
}

static void test_wrap(void)
{
	char lines[3][UI_WRAP_LINE_BYTES];
	int complete = 0, n, i;
	const char *long_title =
		"Juice WRLD - All Girls Are The Same / Lucid Dreams / Robbery (Live at the "
		"Hollywood Palladium, Los Angeles, 2019) [Remastered Extended Edition]";
	n = ui_text_wrap(UI_FONT_TITLE, "Short", 0, 1000, 2, lines, &complete);
	assert(n == 1 && complete && strcmp(lines[0], "Short") == 0);
	n = ui_text_wrap(UI_FONT_TITLE, "  ", 0, 1000, 2, lines, &complete);
	assert(n == 0 && complete);
	n = ui_text_wrap(UI_FONT_HEADING, long_title, 0, 1120, 3, lines, &complete);
	assert(n == 3 && complete);
	for (i = 0; i < n; ++i)
		assert(ui_text_width(UI_FONT_HEADING, lines[i], 0) <= 1120);
	/* every word survives, in order */
	{
		char joined[400] = "";
		for (i = 0; i < n; ++i) {
			if (i) strcat(joined, " ");
			strcat(joined, lines[i]);
		}
		assert(strcmp(joined, long_title) == 0);
	}
	/* not enough room: the last line is ellipsized */
	n = ui_text_wrap(UI_FONT_TITLE, long_title, 0, 500, 2, lines, &complete);
	assert(n == 2 && !complete);
	assert(strcmp(lines[1] + strlen(lines[1]) - 3, "...") == 0);
	assert(ui_text_width(UI_FONT_TITLE, lines[1], 0) <= 500);
	/* an unbreakable word is split between characters, never mid-UTF-8 */
	n = ui_text_wrap(UI_FONT_BODY,
		"\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9",
		0, 60, 3, lines, &complete);
	assert(n >= 2);
	for (i = 0; i < n; ++i) {
		const char *p = lines[i];
		assert(ui_text_width(UI_FONT_BODY, lines[i], 0) <= 60);
		while (*p)
			assert(ui_utf8_next(&p) != 0xfffd);
	}
}

static void test_spectrum(void)
{
	static ui_spectrum s;
	static float l[UI_FFT_SIZE], r[UI_FFT_SIZE];
	int i, best = 0;
	ui_spectrum_init(&s, 48000.0f);
	for (i = 0; i < UI_FFT_SIZE; ++i)
		l[i] = r[i] = 0.5f * sinf(2.0f * 3.14159265f * 1000.0f * (float)i / 48000.0f);
	for (i = 0; i < 30; ++i)
		ui_spectrum_update(&s, l, r, 1.0f / 60.0f);
	for (i = 1; i < UI_BANDS; ++i)
		if (s.bands[i] > s.bands[best])
			best = i;
	/* 1 kHz should land near the middle of a 35 Hz..16 kHz log scale. */
	{
		float expected = (float)UI_BANDS * logf(1000.0f / 35.0f) / logf(16000.0f / 35.0f);
		assert(fabsf((float)best - expected) <= 2.0f);
	}
	assert(s.bands[best] > 0.6f);
	assert(s.bands[2] < 0.25f);
	assert(s.level > 0.3f);
	/* silence decays */
	memset(l, 0, sizeof(l));
	memset(r, 0, sizeof(r));
	for (i = 0; i < 120; ++i)
		ui_spectrum_update(&s, l, r, 1.0f / 60.0f);
	for (i = 0; i < UI_BANDS; ++i)
		assert(s.bands[i] < 0.05f);
	assert(s.silence_time > 1.0f);
}

static void test_tap(void)
{
	static ui_audio_tap tap;
	float block[512];
	float l[300], r[300];
	unsigned int i, round;
	ui_audio_tap_init(&tap);
	for (round = 0; round < 40; ++round) {
		for (i = 0; i < 256; ++i) {
			block[i * 2] = (float)(round * 256 + i);
			block[i * 2 + 1] = -(float)(round * 256 + i);
		}
		ui_audio_tap_push(&tap, block, 256, 2);
	}
	/* newest 300 frames, then 100 frames earlier */
	(void)ui_audio_tap_snapshot(&tap, l, r, 300, 0);
	assert(l[299] == 40.0f * 256.0f - 1.0f && r[299] == -l[299]);
	assert(l[0] == 40.0f * 256.0f - 300.0f);
	(void)ui_audio_tap_snapshot(&tap, l, r, 300, 100);
	assert(l[299] == 40.0f * 256.0f - 101.0f);
	/* absurd delay is clamped inside the ring */
	(void)ui_audio_tap_snapshot(&tap, l, r, 300, 1000000);
	for (i = 1; i < 300; ++i)
		assert(l[i] == l[i - 1] + 1.0f);
}

static void test_settings(void)
{
	int mode = 0, theme = 0, hud = 1;
	const char *path = "build/ui-test-settings.txt";
	assert(ui_settings_parse("mode=3\ntheme=4\nhud=0\n", &mode, &theme, &hud));
	assert(mode == 3 && theme == 4 && hud == 0);
	assert(ui_settings_parse("mode=99\ntheme=-1\nhud=7\ngarbage\n", &mode, &theme, &hud));
	assert(mode == 3 && theme == 4 && hud == 0);
	assert(ui_settings_parse(" mode = 1 \r\ntheme=2", &mode, &theme, &hud));
	assert(mode == 1 && theme == 2);
	if (ui_settings_save(path, 4, 5, 1)) {
		mode = theme = hud = 0;
		assert(ui_settings_load(path, &mode, &theme, &hud));
		assert(mode == 4 && theme == 5 && hud == 1);
		(void)remove(path);
	}
	assert(!ui_settings_load("build/does-not-exist/x.txt", &mode, &theme, &hud));
}

static void test_render(void)
{
	size_t pixels = (size_t)UI_CANVAS_WIDTH * UI_CANVAS_HEIGHT;
	uint32_t *out = malloc(pixels * 4), *bg = malloc(pixels * 4),
		*bg_hud = malloc(pixels * 4);
	static ui_app app;
	static float l[UI_FFT_SIZE], r[UI_FFT_SIZE];
	ui_status st;
	int mode, theme, state, frame, i;
	assert(out && bg && bg_hud);
	for (i = 0; i < UI_FFT_SIZE; ++i) {
		l[i] = 0.9f * sinf((float)i * 0.05f) + 0.3f * sinf((float)i * 0.9f);
		r[i] = -l[i];
	}
	ui_app_init(&app, out, bg, bg_hud);
	memset(&st, 0, sizeof(st));
	(void)snprintf(st.title, sizeof(st.title), "%s",
		"An extraordinarily long title that must be truncated before the right column \xe2\x80\x94 really");
	(void)snprintf(st.artist, sizeof(st.artist), "Artist");
	st.has_progress = 1;
	st.duration_s = 200.0f;
	st.position_s = 250.0f; /* past the end: clamped */
	st.volume_percent = 140; /* out of range: still drawn safely */
	for (state = UI_STATE_IDLE; state <= UI_STATE_PAUSED; ++state) {
		st.state = state;
		ui_app_set_status(&app, &st);
		for (mode = 0; mode < UI_VIS_COUNT; ++mode) {
			for (theme = 0; theme < UI_THEME_COUNT; theme += 2) {
				ui_app_apply_settings(&app, mode, theme, (mode + theme) & 1);
				for (frame = 0; frame < 3; ++frame)
					ui_app_render(&app, l, r, 1.0f / 30.0f, 1000000ULL * (uint64_t)frame);
			}
		}
	}
	/* commands cycle and mark settings dirty */
	app.settings_dirty = 0;
	ui_app_apply_settings(&app, UI_VIS_COUNT - 1, UI_THEME_COUNT - 1, 1);
	ui_app_command(&app, UI_CMD_NEXT_MODE);
	assert(app.mode == 0 && app.settings_dirty);
	ui_app_command(&app, UI_CMD_PREV_MODE);
	assert(app.mode == UI_VIS_COUNT - 1);
	ui_app_command(&app, UI_CMD_NEXT_THEME);
	assert(app.theme == 0);
	ui_app_command(&app, UI_CMD_TOGGLE_HUD);
	assert(app.hud_visible == 0);
	/* a volume change raises the volume indicator */
	st.volume_updates += 1;
	ui_app_set_status(&app, &st);
	assert(app.volume_timer > 0.0f);
	/* bad dt values are tolerated */
	ui_app_render(&app, l, r, -1.0f, 0);
	ui_app_render(&app, l, r, 10.0f, 0);
	free(out);
	free(bg);
	free(bg_hud);
}

int main(void)
{
	test_pixels();
	test_text();
	test_wrap();
	test_spectrum();
	test_tap();
	test_settings();
	test_render();
	printf("ui_test passed\n");
	return 0;
}

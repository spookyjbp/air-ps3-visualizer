#ifndef PS3_AIRPLAY_UI_CANVAS_H
#define PS3_AIRPLAY_UI_CANVAS_H

#include <stddef.h>
#include <stdint.h>

#include "ui_font.h"

/* A 0x00RRGGBB pixel buffer in main memory. stride is in pixels. All drawing
 * is clipped, so callers may pass coordinates partly off-canvas. */
typedef struct {
	uint32_t *px;
	int w;
	int h;
	int stride;
} ui_canvas;

#define UI_RGB(r, g, b) ((((uint32_t)(r) & 0xffU) << 16) | \
	(((uint32_t)(g) & 0xffU) << 8) | ((uint32_t)(b) & 0xffU))

static inline uint32_t ui_rgb_scale(uint32_t c, int alpha)
{
	uint32_t rb = ((c & 0xff00ffU) * (uint32_t)alpha >> 8) & 0xff00ffU;
	uint32_t g = ((c & 0x00ff00U) * (uint32_t)alpha >> 8) & 0x00ff00U;
	return rb | g;
}

/* Per-channel saturating add of two XRGB pixels. */
static inline uint32_t ui_rgb_add(uint32_t a, uint32_t b)
{
	uint32_t rb = (a & 0xff00ffU) + (b & 0xff00ffU);
	uint32_t g = (a & 0x00ff00U) + (b & 0x00ff00U);
	uint32_t mrb = rb & 0x1000100U;
	uint32_t mg = g & 0x10000U;
	rb = (rb | (mrb - (mrb >> 8))) & 0xff00ffU;
	g = (g | (mg - (mg >> 8))) & 0x00ff00U;
	return rb | g;
}

static inline uint32_t ui_rgb_lerp(uint32_t a, uint32_t b, int t)
{
	return ui_rgb_scale(a, 256 - t) + ui_rgb_scale(b, t);
}

/* alpha is 0..256 */
static inline uint32_t ui_rgb_blend(uint32_t dst, uint32_t src, int alpha)
{
	uint32_t drb = dst & 0xff00ffU, dg = dst & 0x00ff00U;
	uint32_t srb = src & 0xff00ffU, sg = src & 0x00ff00U;
	uint32_t rb = (drb + (((srb - drb) * (uint32_t)alpha) >> 8)) & 0xff00ffU;
	uint32_t g = (dg + (((sg - dg) * (uint32_t)alpha) >> 8)) & 0x00ff00U;
	return rb | g;
}

void ui_canvas_fill(ui_canvas *c, uint32_t color);
void ui_canvas_copy(ui_canvas *dst, const ui_canvas *src);
/* dst = a + b (saturating) for every pixel. All three must match in size. */
void ui_canvas_add(ui_canvas *dst, const ui_canvas *a, const ui_canvas *b);
/* Multiplies every channel by keep/256 (trails). keep 0 clears. */
void ui_canvas_fade(ui_canvas *c, int keep);

/* Vertical gradient with a soft radial vignette and ordered dithering. */
void ui_canvas_background(ui_canvas *c, uint32_t top, uint32_t bottom,
	uint32_t glow, float glow_x, float glow_y, float glow_radius);

/* Alpha-blended solid shapes (alpha 0..255). */
void ui_rect_blend(ui_canvas *c, int x, int y, int w, int h, uint32_t color,
	int alpha);
void ui_round_rect_blend(ui_canvas *c, float x, float y, float w, float h,
	float radius, uint32_t color, int alpha);
void ui_circle_blend(ui_canvas *c, float cx, float cy, float radius,
	uint32_t color, int alpha);
void ui_triangle_blend(ui_canvas *c, const float xs[3], const float ys[3],
	uint32_t color, int alpha);

/* Additive light shapes (alpha 0..255 scales the color). */
void ui_round_rect_add(ui_canvas *c, float x, float y, float w, float h,
	float radius, uint32_t color_top, uint32_t color_bottom, int alpha);
void ui_line_add(ui_canvas *c, float x0, float y0, float x1, float y1,
	float width, uint32_t color, int alpha);
void ui_ring_add(ui_canvas *c, float cx, float cy, float radius, float width,
	uint32_t color, int alpha);
void ui_glow_ellipse_add(ui_canvas *c, float cx, float cy, float rx, float ry,
	uint32_t color, int alpha);
void ui_glow_add(ui_canvas *c, float cx, float cy, float radius,
	uint32_t color, int alpha);
void ui_vspan_add(ui_canvas *c, int x, float y0, float y1, uint32_t color,
	int alpha_top, int alpha_bottom);

/* Text. y is the baseline. tracking is extra spacing in quarter pixels. */
int ui_text_width(ui_font_id font, const char *utf8, int tracking);
int ui_text_draw(ui_canvas *c, ui_font_id font, int x, int y,
	const char *utf8, uint32_t color, int alpha, int tracking);
/* Word-wraps text into at most max_lines lines of max_width pixels. Words
 * longer than a line are broken between characters. If text is left over,
 * the last line ends with "...". Returns the number of lines; *complete is
 * set to 1 when nothing had to be cut. */
#define UI_WRAP_LINE_BYTES 160
int ui_text_wrap(ui_font_id font, const char *utf8, int tracking, int max_width,
	int max_lines, char lines[][UI_WRAP_LINE_BYTES], int *complete);
/* Copies text into out, truncating with an ellipsis to fit max_width. */
void ui_text_fit(ui_font_id font, const char *utf8, int tracking,
	int max_width, char *out, size_t out_size);

#endif

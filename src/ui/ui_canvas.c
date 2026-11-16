#include "ui_canvas.h"

#include <math.h>
#include <string.h>

static inline int clampi(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static inline float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static inline void add_px(uint32_t *p, uint32_t color, int alpha)
{
	if (alpha > 0)
		*p = ui_rgb_add(*p, ui_rgb_scale(color, alpha > 256 ? 256 : alpha));
}

void ui_canvas_fill(ui_canvas *c, uint32_t color)
{
	int x, y;
	for (y = 0; y < c->h; ++y) {
		uint32_t *row = c->px + (size_t)y * (size_t)c->stride;
		if (color == 0) {
			memset(row, 0, (size_t)c->w * sizeof(uint32_t));
			continue;
		}
		for (x = 0; x < c->w; ++x)
			row[x] = color;
	}
}

void ui_canvas_copy(ui_canvas *dst, const ui_canvas *src)
{
	int y;
	int w = dst->w < src->w ? dst->w : src->w;
	int h = dst->h < src->h ? dst->h : src->h;
	for (y = 0; y < h; ++y)
		memcpy(dst->px + (size_t)y * (size_t)dst->stride,
			src->px + (size_t)y * (size_t)src->stride,
			(size_t)w * sizeof(uint32_t));
}

void ui_canvas_add(ui_canvas *dst, const ui_canvas *a, const ui_canvas *b)
{
	int x, y;
	for (y = 0; y < dst->h; ++y) {
		uint32_t *d = dst->px + (size_t)y * (size_t)dst->stride;
		const uint32_t *pa = a->px + (size_t)y * (size_t)a->stride;
		const uint32_t *pb = b->px + (size_t)y * (size_t)b->stride;
		for (x = 0; x < dst->w; ++x) {
			uint32_t v = pb[x];
			d[x] = v ? ui_rgb_add(pa[x], v) : pa[x];
		}
	}
}

void ui_canvas_fade(ui_canvas *c, int keep)
{
	int x, y;
	if (keep <= 0) {
		ui_canvas_fill(c, 0);
		return;
	}
	for (y = 0; y < c->h; ++y) {
		uint32_t *row = c->px + (size_t)y * (size_t)c->stride;
		for (x = 0; x < c->w; ++x) {
			uint32_t v = row[x];
			if (v)
				row[x] = ui_rgb_scale(v, keep);
		}
	}
}

void ui_canvas_background(ui_canvas *c, uint32_t top, uint32_t bottom,
	uint32_t glow, float glow_x, float glow_y, float glow_radius)
{
	static const float bayer[4][4] = {
		{ 0.0f, 8.0f, 2.0f, 10.0f }, { 12.0f, 4.0f, 14.0f, 6.0f },
		{ 3.0f, 11.0f, 1.0f, 9.0f }, { 15.0f, 7.0f, 13.0f, 5.0f }
	};
	float tr = (float)((top >> 16) & 0xff), tg = (float)((top >> 8) & 0xff),
		tb = (float)(top & 0xff);
	float br = (float)((bottom >> 16) & 0xff), bg = (float)((bottom >> 8) & 0xff),
		bb = (float)(bottom & 0xff);
	float gr = (float)((glow >> 16) & 0xff), gg = (float)((glow >> 8) & 0xff),
		gb = (float)(glow & 0xff);
	float cx = (float)c->w * 0.5f, cy = (float)c->h * 0.5f;
	float inv_diag = 1.0f / sqrtf(cx * cx + cy * cy);
	float gx = glow_x * (float)c->w, gy = glow_y * (float)c->h;
	float grad = glow_radius * (float)c->w;
	int x, y;
	for (y = 0; y < c->h; ++y) {
		uint32_t *row = c->px + (size_t)y * (size_t)c->stride;
		float t = (float)y / (float)(c->h > 1 ? c->h - 1 : 1);
		float r0 = tr + (br - tr) * t, g0 = tg + (bg - tg) * t,
			b0 = tb + (bb - tb) * t;
		for (x = 0; x < c->w; ++x) {
			float dx = ((float)x - cx) * inv_diag, dy = ((float)y - cy) * inv_diag;
			float vignette = 1.0f - 0.55f * (dx * dx + dy * dy) * 2.0f;
			float ex = ((float)x - gx) / grad, ey = ((float)y - gy) / grad;
			float d2 = ex * ex + ey * ey;
			float halo = d2 < 1.0f ? (1.0f - d2) * (1.0f - d2) : 0.0f;
			float dither = (bayer[y & 3][x & 3] - 7.5f) / 16.0f;
			float r = r0 * vignette + gr * halo + dither;
			float g = g0 * vignette + gg * halo + dither;
			float b = b0 * vignette + gb * halo + dither;
			row[x] = UI_RGB(clampi((int)(r + 0.5f), 0, 255),
				clampi((int)(g + 0.5f), 0, 255), clampi((int)(b + 0.5f), 0, 255));
		}
	}
}

void ui_rect_blend(ui_canvas *c, int x, int y, int w, int h, uint32_t color,
	int alpha)
{
	int x0 = clampi(x, 0, c->w), x1 = clampi(x + w, 0, c->w);
	int y0 = clampi(y, 0, c->h), y1 = clampi(y + h, 0, c->h);
	int a = alpha + (alpha >> 7);
	int i, j;
	for (j = y0; j < y1; ++j) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		for (i = x0; i < x1; ++i)
			row[i] = ui_rgb_blend(row[i], color, a);
	}
}

/* Signed distance from (px,py) to a rounded box centered at the origin. */
static inline float round_box_sdf(float px, float py, float hx, float hy,
	float r)
{
	float qx = fabsf(px) - (hx - r), qy = fabsf(py) - (hy - r);
	float ox = qx > 0.0f ? qx : 0.0f, oy = qy > 0.0f ? qy : 0.0f;
	float inside = qx > qy ? qx : qy;
	return sqrtf(ox * ox + oy * oy) + (inside < 0.0f ? inside : 0.0f) - r;
}

void ui_round_rect_blend(ui_canvas *c, float x, float y, float w, float h,
	float radius, uint32_t color, int alpha)
{
	float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
	int x0 = clampi((int)floorf(x) - 1, 0, c->w), x1 = clampi((int)ceilf(x + w) + 1, 0, c->w);
	int y0 = clampi((int)floorf(y) - 1, 0, c->h), y1 = clampi((int)ceilf(y + h) + 1, 0, c->h);
	int i, j;
	radius = clampf(radius, 0.0f, hx < hy ? hx : hy);
	for (j = y0; j < y1; ++j) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		for (i = x0; i < x1; ++i) {
			float d = round_box_sdf((float)i + 0.5f - cx, (float)j + 0.5f - cy,
				hx, hy, radius);
			float cov = clampf(0.5f - d, 0.0f, 1.0f);
			if (cov > 0.0f)
				row[i] = ui_rgb_blend(row[i], color, (int)(cov * (float)alpha * 1.004f));
		}
	}
}

void ui_circle_blend(ui_canvas *c, float cx, float cy, float radius,
	uint32_t color, int alpha)
{
	ui_round_rect_blend(c, cx - radius, cy - radius, radius * 2.0f,
		radius * 2.0f, radius, color, alpha);
}

void ui_triangle_blend(ui_canvas *c, const float xs[3], const float ys[3],
	uint32_t color, int alpha)
{
	float minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
	float nx[3], ny[3], nd[3];
	float area = (xs[1] - xs[0]) * (ys[2] - ys[0]) - (ys[1] - ys[0]) * (xs[2] - xs[0]);
	float sign = area < 0.0f ? -1.0f : 1.0f;
	int e, i, j, x0, x1, y0, y1;
	for (e = 1; e < 3; ++e) {
		if (xs[e] < minx) minx = xs[e];
		if (xs[e] > maxx) maxx = xs[e];
		if (ys[e] < miny) miny = ys[e];
		if (ys[e] > maxy) maxy = ys[e];
	}
	for (e = 0; e < 3; ++e) {
		int n = (e + 1) % 3;
		float dx = xs[n] - xs[e], dy = ys[n] - ys[e];
		float len = sqrtf(dx * dx + dy * dy);
		if (len <= 0.0f)
			return;
		/* inward normal */
		nx[e] = -dy / len * sign;
		ny[e] = dx / len * sign;
		nd[e] = -(nx[e] * xs[e] + ny[e] * ys[e]);
	}
	x0 = clampi((int)floorf(minx) - 1, 0, c->w); x1 = clampi((int)ceilf(maxx) + 1, 0, c->w);
	y0 = clampi((int)floorf(miny) - 1, 0, c->h); y1 = clampi((int)ceilf(maxy) + 1, 0, c->h);
	for (j = y0; j < y1; ++j) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		for (i = x0; i < x1; ++i) {
			float px = (float)i + 0.5f, py = (float)j + 0.5f;
			float d = 1e9f;
			for (e = 0; e < 3; ++e) {
				float v = nx[e] * px + ny[e] * py + nd[e];
				if (v < d) d = v;
			}
			d = clampf(d + 0.5f, 0.0f, 1.0f);
			if (d > 0.0f)
				row[i] = ui_rgb_blend(row[i], color, (int)(d * (float)alpha * 1.004f));
		}
	}
}

void ui_round_rect_add(ui_canvas *c, float x, float y, float w, float h,
	float radius, uint32_t color_top, uint32_t color_bottom, int alpha)
{
	float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
	int x0 = clampi((int)floorf(x), 0, c->w), x1 = clampi((int)ceilf(x + w), 0, c->w);
	int y0 = clampi((int)floorf(y), 0, c->h), y1 = clampi((int)ceilf(y + h), 0, c->h);
	float colcov[64];
	int i, j, n = x1 - x0;
	if (n <= 0 || y1 <= y0 || alpha <= 0)
		return;
	radius = clampf(radius, 0.0f, hx < hy ? hx : hy);
	if (n <= 64) {
		for (i = 0; i < n; ++i) {
			float px = (float)(x0 + i) + 0.5f;
			float left = px - x, right = x + w - px;
			colcov[i] = clampf((left < right ? left : right) + 0.5f, 0.0f, 1.0f);
		}
	}
	for (j = y0; j < y1; ++j) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		float py = (float)j + 0.5f;
		int t = (int)(256.0f * (py - y) / (h > 1.0f ? h : 1.0f));
		uint32_t color = ui_rgb_lerp(color_top, color_bottom, clampi(t, 0, 256));
		int corner = py < y + radius || py > y + h - radius || n > 64;
		float vcov = clampf((py - y < y + h - py ? py - y : y + h - py) + 0.5f, 0.0f, 1.0f);
		for (i = x0; i < x1; ++i) {
			float cov;
			if (corner)
				cov = clampf(0.5f - round_box_sdf(
					(float)i + 0.5f - cx, py - cy, hx, hy, radius), 0.0f, 1.0f);
			else
				cov = colcov[i - x0] * vcov;
			add_px(&row[i], color, (int)(cov * (float)alpha));
		}
	}
}

void ui_line_add(ui_canvas *c, float x0, float y0, float x1, float y1,
	float width, uint32_t color, int alpha)
{
	float half = width * 0.5f;
	float dx = x1 - x0, dy = y1 - y0;
	float len2 = dx * dx + dy * dy;
	float inv = len2 > 1e-6f ? 1.0f / len2 : 0.0f;
	int bx0 = clampi((int)floorf((x0 < x1 ? x0 : x1) - half - 1.0f), 0, c->w);
	int bx1 = clampi((int)ceilf((x0 > x1 ? x0 : x1) + half + 1.0f), 0, c->w);
	int by0 = clampi((int)floorf((y0 < y1 ? y0 : y1) - half - 1.0f), 0, c->h);
	int by1 = clampi((int)ceilf((y0 > y1 ? y0 : y1) + half + 1.0f), 0, c->h);
	int i, j;
	if (alpha <= 0)
		return;
	for (j = by0; j < by1; ++j) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		float py = (float)j + 0.5f - y0;
		for (i = bx0; i < bx1; ++i) {
			float px = (float)i + 0.5f - x0;
			float t = clampf((px * dx + py * dy) * inv, 0.0f, 1.0f);
			float ex = px - t * dx, ey = py - t * dy;
			float d = sqrtf(ex * ex + ey * ey);
			float cov = clampf(half + 0.5f - d, 0.0f, 1.0f);
			if (cov > 0.0f)
				add_px(&row[i], color, (int)(cov * (float)alpha));
		}
	}
}

void ui_ring_add(ui_canvas *c, float cx, float cy, float radius, float width,
	uint32_t color, int alpha)
{
	float outer = radius + width * 0.5f + 1.0f;
	float inner = radius - width * 0.5f - 1.0f;
	int y0 = clampi((int)floorf(cy - outer), 0, c->h);
	int y1 = clampi((int)ceilf(cy + outer), 0, c->h);
	int j;
	if (alpha <= 0 || radius <= 0.0f)
		return;
	for (j = y0; j < y1; ++j) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		float dy = (float)j + 0.5f - cy;
		float xo2 = outer * outer - dy * dy;
		float xi2 = inner * inner - dy * dy;
		float xo, xi;
		int side;
		if (xo2 <= 0.0f)
			continue;
		xo = sqrtf(xo2);
		xi = xi2 > 0.0f ? sqrtf(xi2) : 0.0f;
		for (side = 0; side < 2; ++side) {
			float a = side ? cx + xi : cx - xo;
			float b = side ? cx + xo : cx - xi;
			int i0 = clampi((int)floorf(a), 0, c->w), i1 = clampi((int)ceilf(b), 0, c->w);
			int i;
			if (xi <= 0.0f && side)
				break;
			if (xi <= 0.0f)
				i1 = clampi((int)ceilf(cx + xo), 0, c->w);
			for (i = i0; i < i1; ++i) {
				float dx = (float)i + 0.5f - cx;
				float d = fabsf(sqrtf(dx * dx + dy * dy) - radius);
				float cov = clampf(width * 0.5f + 0.5f - d, 0.0f, 1.0f);
				if (cov > 0.0f)
					add_px(&row[i], color, (int)(cov * (float)alpha));
			}
		}
	}
}

void ui_glow_ellipse_add(ui_canvas *c, float cx, float cy, float rx, float ry,
	uint32_t color, int alpha)
{
	int y0 = clampi((int)floorf(cy - ry), 0, c->h), y1 = clampi((int)ceilf(cy + ry), 0, c->h);
	float inv_rx = rx > 0.0f ? 1.0f / rx : 0.0f, inv_ry = ry > 0.0f ? 1.0f / ry : 0.0f;
	int i, j;
	if (alpha <= 0 || rx <= 0.0f || ry <= 0.0f)
		return;
	for (j = y0; j < y1; ++j) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		float ny = ((float)j + 0.5f - cy) * inv_ry;
		float rem = 1.0f - ny * ny;
		float half;
		int x0, x1;
		if (rem <= 0.0f)
			continue;
		half = rx * sqrtf(rem);
		x0 = clampi((int)floorf(cx - half), 0, c->w);
		x1 = clampi((int)ceilf(cx + half), 0, c->w);
		for (i = x0; i < x1; ++i) {
			float nx = ((float)i + 0.5f - cx) * inv_rx;
			float f = rem - nx * nx;
			if (f > 0.0f) {
				int a = (int)(f * f * (float)alpha);
				if (a > 0)
					row[i] = ui_rgb_add(row[i], ui_rgb_scale(color, a));
			}
		}
	}
}

void ui_glow_add(ui_canvas *c, float cx, float cy, float radius,
	uint32_t color, int alpha)
{
	ui_glow_ellipse_add(c, cx, cy, radius, radius, color, alpha);
}

void ui_vspan_add(ui_canvas *c, int x, float y0, float y1, uint32_t color,
	int alpha_top, int alpha_bottom)
{
	float top = y0 < y1 ? y0 : y1, bottom = y0 < y1 ? y1 : y0;
	int at = y0 < y1 ? alpha_top : alpha_bottom;
	int ab = y0 < y1 ? alpha_bottom : alpha_top;
	int j0, j1, j;
	float span;
	if (x < 0 || x >= c->w)
		return;
	if (bottom - top < 1.0f) {
		float mid = (top + bottom) * 0.5f;
		top = mid - 0.5f;
		bottom = mid + 0.5f;
	}
	span = bottom - top;
	j0 = clampi((int)floorf(top), 0, c->h);
	j1 = clampi((int)ceilf(bottom), 0, c->h);
	for (j = j0; j < j1; ++j) {
		float py0 = (float)j, py1 = (float)j + 1.0f;
		float cov = (py1 < bottom ? py1 : bottom) - (py0 > top ? py0 : top);
		float t = ((float)j + 0.5f - top) / span;
		int a = (int)((float)at + (float)(ab - at) * clampf(t, 0.0f, 1.0f));
		if (cov > 0.0f)
			add_px(&c->px[(size_t)j * (size_t)c->stride + (size_t)x], color,
				(int)(cov * (float)a));
	}
}

static int glyph_advance(const ui_font *font, int index, int tracking)
{
	return font->glyphs[index].advance_q2 + tracking;
}

int ui_text_width(ui_font_id id, const char *utf8, int tracking)
{
	const ui_font *font = ui_font_get(id);
	const char *p = utf8;
	int width_q2 = 0, last_tracking = 0;
	if (utf8 == NULL)
		return 0;
	while (*p) {
		uint32_t cp = ui_utf8_next(&p);
		width_q2 += glyph_advance(font, ui_font_glyph_index(cp), tracking);
		last_tracking = tracking;
	}
	return (width_q2 - last_tracking + 2) / 4;
}

int ui_text_draw(ui_canvas *c, ui_font_id id, int x, int y,
	const char *utf8, uint32_t color, int alpha, int tracking)
{
	const ui_font *font = ui_font_get(id);
	const char *p = utf8;
	int pen_q2 = x * 4;
	if (utf8 == NULL || alpha <= 0)
		return x;
	while (*p) {
		uint32_t cp = ui_utf8_next(&p);
		int index = ui_font_glyph_index(cp);
		const ui_glyph *g = &font->glyphs[index];
		int gx = (pen_q2 + 2) / 4 + g->x, gy = y + g->y;
		int i, j;
		for (j = 0; j < g->h; ++j) {
			int py = gy + j;
			uint32_t *row;
			if (py < 0 || py >= c->h)
				continue;
			row = c->px + (size_t)py * (size_t)c->stride;
			for (i = 0; i < g->w; ++i) {
				int px = gx + i;
				size_t n = (size_t)j * (size_t)g->w + (size_t)i;
				uint8_t packed = font->alpha[g->offset + n / 2];
				int a = (n & 1) ? (packed & 0x0f) : (packed >> 4);
				if (a == 0 || px < 0 || px >= c->w)
					continue;
				row[px] = ui_rgb_blend(row[px], color, (a * 17 * alpha) / 255 + (a == 15 && alpha == 255));
			}
		}
		pen_q2 += glyph_advance(font, index, tracking);
	}
	return (pen_q2 + 2) / 4;
}

void ui_text_fit(ui_font_id id, const char *utf8, int tracking,
	int max_width, char *out, size_t out_size)
{
	size_t length;
	if (out_size == 0)
		return;
	out[0] = '\0';
	if (utf8 == NULL)
		return;
	length = strlen(utf8);
	if (length >= out_size)
		length = out_size - 1;
	memcpy(out, utf8, length);
	out[length] = '\0';
	if (ui_text_width(id, out, tracking) <= max_width)
		return;
	/* Trim whole UTF-8 sequences until "text…" fits (… folds to "..."). */
	while (length > 0) {
		do {
			--length;
		} while (length > 0 && ((unsigned char)out[length] & 0xc0) == 0x80);
		while (length > 0 && out[length - 1] == ' ')
			--length;
		if (length + 4 > out_size - 1)
			continue;
		memcpy(out + length, "...", 4);
		if (ui_text_width(id, out, tracking) <= max_width)
			return;
	}
	out[0] = '\0';
}

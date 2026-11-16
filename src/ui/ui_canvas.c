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
	/* Branch-free: the PPU is in-order and mispredicts are expensive. */
	for (y = 0; y < dst->h; ++y) {
		uint32_t *d = dst->px + (size_t)y * (size_t)dst->stride;
		const uint32_t *pa = a->px + (size_t)y * (size_t)a->stride;
		const uint32_t *pb = b->px + (size_t)y * (size_t)b->stride;
		for (x = 0; x < dst->w; ++x)
			d[x] = ui_rgb_add(pa[x], pb[x]);
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
		for (x = 0; x < c->w; ++x)
			row[x] = ui_rgb_scale(row[x], keep);
	}
}

void ui_canvas_background(ui_canvas *c, uint32_t top, uint32_t bottom,
	uint32_t glow, float glow_x, float glow_y, float glow_radius)
{
	/* 4x4 ordered dither, in 1/16 units centred on zero. */
	static const int bayer[4][4] = {
		{ -8, 0, -6, 2 }, { 4, -4, 6, -2 }, { -5, 3, -7, 1 }, { 7, -1, 5, -3 }
	};
	int tr = (int)((top >> 16) & 0xff), tg = (int)((top >> 8) & 0xff), tb = (int)(top & 0xff);
	int br = (int)((bottom >> 16) & 0xff), bgc = (int)((bottom >> 8) & 0xff), bb = (int)(bottom & 0xff);
	int gr = (int)((glow >> 16) & 0xff), gg = (int)((glow >> 8) & 0xff), gb = (int)(glow & 0xff);
	int cx = c->w / 2, cy = c->h / 2;
	int gx = (int)(glow_x * (float)c->w), gy = (int)(glow_y * (float)c->h);
	int grad = (int)(glow_radius * (float)c->w);
	/* vignette = 1 - 1.1 * d^2 / diag^2, halo = (1 - e^2/grad^2)^2, all q16 */
	int64_t diag2 = (int64_t)cx * cx + (int64_t)cy * cy;
	int64_t grad2 = (int64_t)grad * grad;
	int64_t vig_k, halo_k;
	int x, y;
	if (grad2 < 1)
		grad2 = 1;
	if (diag2 < 1)
		diag2 = 1;
	/* reciprocals so the per-pixel loop has no 64-bit divides */
	vig_k = ((int64_t)72090 << 32) / diag2;     /* 1.1 in q16, scaled by 2^32 */
	halo_k = ((int64_t)1 << 48) / grad2;        /* q16 result, scaled by 2^32 */
	for (y = 0; y < c->h; ++y) {
		uint32_t *row = c->px + (size_t)y * (size_t)c->stride;
		int t16 = (int)(((int64_t)y << 16) / (c->h > 1 ? c->h - 1 : 1));
		/* row base colour in 8.8 fixed point */
		int r0 = (tr << 8) + (((br - tr) * t16) >> 8);
		int g0 = (tg << 8) + (((bgc - tg) * t16) >> 8);
		int b0 = (tb << 8) + (((bb - tb) * t16) >> 8);
		int64_t dy2 = (int64_t)(y - cy) * (y - cy);
		int64_t ey2 = (int64_t)(y - gy) * (y - gy);
		for (x = 0; x < c->w; ++x) {
			int64_t dx = x - cx, ex = x - gx;
			int vig = 65536 - (int)(((dx * dx + dy2) * vig_k) >> 32);
			int64_t e2 = ((ex * ex + ey2) * halo_k) >> 32;
			int halo = e2 < 65536 ? (int)(((65536 - e2) * (65536 - e2)) >> 16) : 0;
			int d = bayer[y & 3][x & 3] * 16; /* +-0.5 in 8.8 */
			int r = (int)(((int64_t)r0 * vig) >> 16) + ((gr * halo) >> 8) + d;
			int g = (int)(((int64_t)g0 * vig) >> 16) + ((gg * halo) >> 8) + d;
			int b = (int)(((int64_t)b0 * vig) >> 16) + ((gb * halo) >> 8) + d;
			row[x] = UI_RGB(clampi((r + 128) >> 8, 0, 255), clampi((g + 128) >> 8, 0, 255),
				clampi((b + 128) >> 8, 0, 255));
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
	int colcov[96];
	int i, j, n = x1 - x0;
	int t_q16, dt_q16;
	if (n <= 0 || y1 <= y0 || alpha <= 0)
		return;
	if (n > 96) {
		/* wide shapes are rare: split into vertical slices */
		int mid = x0 + n / 2;
		ui_round_rect_add(c, x, y, (float)mid - x, h, 0.0f, color_top, color_bottom, alpha);
		ui_round_rect_add(c, (float)mid, y, x + w - (float)mid, h, 0.0f, color_top,
			color_bottom, alpha);
		return;
	}
	radius = clampf(radius, 0.0f, hx < hy ? hx : hy);
	/* Integer per-column coverage; the per-pixel loop below has no float
	 * math (float->int conversion stalls the PPU on every pixel). */
	for (i = 0; i < n; ++i) {
		float px = (float)(x0 + i) + 0.5f;
		float left = px - x, right = x + w - px;
		colcov[i] = (int)(clampf((left < right ? left : right) + 0.5f, 0.0f, 1.0f) * 256.0f);
	}
	t_q16 = (int)(65536.0f * ((float)y0 + 0.5f - y) / (h > 1.0f ? h : 1.0f));
	dt_q16 = (int)(65536.0f / (h > 1.0f ? h : 1.0f));
	for (j = y0; j < y1; ++j, t_q16 += dt_q16) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		float py = (float)j + 0.5f;
		uint32_t color = ui_rgb_lerp(color_top, color_bottom,
			clampi(t_q16 >> 8, 0, 256));
		if (py < y + radius || py > y + h - radius) {
			for (i = x0; i < x1; ++i) {
				float cov = clampf(0.5f - round_box_sdf(
					(float)i + 0.5f - cx, py - cy, hx, hy, radius), 0.0f, 1.0f);
				add_px(&row[i], color, (int)(cov * (float)alpha));
			}
		} else {
			int vcov = (int)(clampf((py - y < y + h - py ? py - y : y + h - py) + 0.5f,
				0.0f, 1.0f) * (float)alpha);
			for (i = 0; i < n; ++i)
				add_px(&row[x0 + i], color, (colcov[i] * vcov) >> 8);
		}
	}
}

/* Coverage of one pixel at squared distance d2 from a line of half width
 * `half`; returns 0..alpha. Fully covered pixels need no sqrt/conversion. */
static inline int line_alpha(float d2, float inner2, float outer2, float half,
	int alpha)
{
	if (d2 <= inner2)
		return alpha;
	if (d2 >= outer2)
		return 0;
	return (int)((half + 0.5f - sqrtf(d2)) * (float)alpha);
}

void ui_line_add(ui_canvas *c, float x0, float y0, float x1, float y1,
	float width, uint32_t color, int alpha)
{
	float half = width * 0.5f;
	float inner = half - 0.5f > 0.0f ? half - 0.5f : 0.0f;
	float inner2 = inner * inner, outer2 = (half + 0.5f) * (half + 0.5f);
	float dx = x1 - x0, dy = y1 - y0;
	float len2 = dx * dx + dy * dy;
	float inv = len2 > 1e-6f ? 1.0f / len2 : 0.0f;
	float len = sqrtf(len2);
	int bx0 = clampi((int)floorf((x0 < x1 ? x0 : x1) - half - 1.0f), 0, c->w);
	int bx1 = clampi((int)ceilf((x0 > x1 ? x0 : x1) + half + 1.0f), 0, c->w);
	int by0 = clampi((int)floorf((y0 < y1 ? y0 : y1) - half - 1.0f), 0, c->h);
	int by1 = clampi((int)ceilf((y0 > y1 ? y0 : y1) + half + 1.0f), 0, c->h);
	int i, j;
	if (alpha <= 0 || bx1 <= bx0 || by1 <= by0)
		return;
	/* Visit only a band around the segment rather than its bounding box. */
	if (fabsf(dy) >= fabsf(dx)) {
		float ext = (half + 1.5f) * (len > 0.0f ? len / fabsf(dy > 0.0f || dy < 0.0f ? dy : 1.0f) : 1.0f);
		for (j = by0; j < by1; ++j) {
			uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
			float py = (float)j + 0.5f - y0;
			float s = dy != 0.0f ? clampf(py / dy, 0.0f, 1.0f) : 0.0f;
			float xc = x0 + s * dx;
			int i0 = clampi((int)(xc - ext), bx0, bx1), i1 = clampi((int)(xc + ext) + 1, bx0, bx1);
			for (i = i0; i < i1; ++i) {
				float px = (float)i + 0.5f - x0;
				float t = clampf((px * dx + py * dy) * inv, 0.0f, 1.0f);
				float ex = px - t * dx, ey = py - t * dy;
				add_px(&row[i], color, line_alpha(ex * ex + ey * ey, inner2, outer2, half, alpha));
			}
		}
	} else {
		float ext = (half + 1.5f) * (len / fabsf(dx));
		for (i = bx0; i < bx1; ++i) {
			float px = (float)i + 0.5f - x0;
			float s = clampf(px / dx, 0.0f, 1.0f);
			float yc = y0 + s * dy;
			int j0 = clampi((int)(yc - ext), by0, by1), j1 = clampi((int)(yc + ext) + 1, by0, by1);
			for (j = j0; j < j1; ++j) {
				float py = (float)j + 0.5f - y0;
				float t = clampf((px * dx + py * dy) * inv, 0.0f, 1.0f);
				float ex = px - t * dx, ey = py - t * dy;
				add_px(&c->px[(size_t)j * (size_t)c->stride + (size_t)i], color,
					line_alpha(ex * ex + ey * ey, inner2, outer2, half, alpha));
			}
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
	float inv_ry = ry > 0.0f ? 1.0f / ry : 0.0f;
	int icx = (int)floorf(cx);
	int inv_rx2_q24;
	int j;
	if (alpha <= 0 || rx < 0.5f || ry < 0.5f)
		return;
	if (alpha > 255)
		alpha = 255;
	inv_rx2_q24 = (int)(16777216.0f / (rx * rx));
	/* Fixed point inside the row: falloff (1 - d^2)^2 with no float math. */
	for (j = y0; j < y1; ++j) {
		uint32_t *row = c->px + (size_t)j * (size_t)c->stride;
		float ny = ((float)j + 0.5f - cy) * inv_ry;
		float rem = 1.0f - ny * ny;
		int rem_q16, half, i0, i1, i;
		if (rem <= 0.0f)
			continue;
		rem_q16 = (int)(rem * 65536.0f);
		half = (int)(rx * sqrtf(rem)) + 1;
		i0 = clampi(icx - half, 0, c->w);
		i1 = clampi(icx + half + 1, 0, c->w);
		for (i = i0; i < i1; ++i) {
			int d = i - icx;
			int f = rem_q16 - ((d * d * inv_rx2_q24) >> 8);
			if (f > 0) {
				int f8 = f >> 8;
				int a = (f8 * f8 * alpha) >> 16;
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
	int j0, j1, j, a_q16, da_q16;
	float span;
	uint32_t *p;
	if (x < 0 || x >= c->w || (at <= 0 && ab <= 0))
		return;
	if (bottom - top < 1.0f) {
		float mid = (top + bottom) * 0.5f;
		top = mid - 0.5f;
		bottom = mid + 0.5f;
	}
	span = bottom - top;
	j0 = (int)floorf(top);
	j1 = (int)ceilf(bottom);
	da_q16 = (int)((float)(ab - at) * 65536.0f / span);
	a_q16 = (int)(((float)at + (float)(ab - at) * (((float)j0 + 0.5f - top) / span)) * 65536.0f);
	if (j0 < 0) {
		a_q16 += da_q16 * (0 - j0);
		j0 = 0;
	}
	if (j1 > c->h)
		j1 = c->h;
	if (j0 >= j1)
		return;
	p = &c->px[(size_t)j0 * (size_t)c->stride + (size_t)x];
	/* Partial coverage on the end rows only; integer alpha steps between. */
	{
		float first = (float)j0 + 1.0f < bottom ? (float)j0 + 1.0f - top : span;
		int a = (int)(first * (float)(a_q16 >> 16));
		add_px(p, color, a);
		p += c->stride;
		a_q16 += da_q16;
	}
	for (j = j0 + 1; j < j1 - 1; ++j, a_q16 += da_q16, p += c->stride) {
		int a = a_q16 >> 16;
		if (a > 0)
			*p = ui_rgb_add(*p, ui_rgb_scale(color, a > 256 ? 256 : a));
	}
	if (j1 - 1 > j0) {
		float last = bottom - (float)(j1 - 1);
		add_px(p, color, (int)(last * (float)(a_q16 >> 16)));
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
	/* alpha 0..255 -> blend weight 0..256 per 4-bit glyph level */
	int text_full = alpha + (alpha >> 7);
	int text_step = (text_full * 256) / 15;
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
				row[px] = ui_rgb_blend(row[px], color, a == 15 ? text_full : (a * text_step) >> 8);
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

static int width_of_range(ui_font_id font, const char *start, size_t length,
	int tracking)
{
	char buffer[UI_WRAP_LINE_BYTES];
	if (length >= sizeof(buffer))
		length = sizeof(buffer) - 1;
	memcpy(buffer, start, length);
	buffer[length] = '\0';
	return ui_text_width(font, buffer, tracking);
}

static void copy_range(char *out, const char *start, size_t length)
{
	if (length >= UI_WRAP_LINE_BYTES)
		length = UI_WRAP_LINE_BYTES - 1;
	/* never end on a partial UTF-8 sequence */
	while (length > 0 && ((unsigned char)start[length] & 0xc0) == 0x80 &&
		start[length] != '\0')
		--length;
	memcpy(out, start, length);
	out[length] = '\0';
	while (length > 0 && out[length - 1] == ' ')
		out[--length] = '\0';
}

int ui_text_wrap(ui_font_id font, const char *utf8, int tracking, int max_width,
	int max_lines, char lines[][UI_WRAP_LINE_BYTES], int *complete)
{
	const char *p = utf8;
	int count = 0;
	if (complete != NULL)
		*complete = 1;
	if (utf8 == NULL || max_lines <= 0)
		return 0;
	while (count < max_lines) {
		const char *best = NULL, *scan;
		while (*p == ' ')
			++p;
		if (*p == '\0')
			break;
		/* longest run of whole words that fits */
		for (scan = p;; ) {
			const char *end = scan;
			while (*end != '\0' && *end != ' ')
				++end;
			if ((size_t)(end - p) >= UI_WRAP_LINE_BYTES - 4 ||
				width_of_range(font, p, (size_t)(end - p), tracking) > max_width)
				break;
			best = end;
			if (*end == '\0')
				break;
			scan = end + 1;
		}
		if (best == NULL) {
			/* a single word wider than the line: break between characters */
			const char *q = p, *last = NULL;
			while (*q != '\0' && *q != ' ') {
				const char *next = q;
				(void)ui_utf8_next(&next);
				if ((size_t)(next - p) >= UI_WRAP_LINE_BYTES - 4 ||
					width_of_range(font, p, (size_t)(next - p), tracking) > max_width)
					break;
				last = next;
				q = next;
			}
			if (last == NULL) {
				last = p;
				(void)ui_utf8_next(&last);
			}
			best = last;
		}
		if (count == max_lines - 1) {
			const char *rest = best;
			while (*rest == ' ')
				++rest;
			if (*rest != '\0') {
				/* out of lines: ellipsize the remainder onto the last line */
				ui_text_fit(font, p, tracking, max_width, lines[count],
					UI_WRAP_LINE_BYTES);
				if (complete != NULL)
					*complete = 0;
				return count + 1;
			}
		}
		copy_range(lines[count], p, (size_t)(best - p));
		++count;
		p = best;
	}
	return count;
}

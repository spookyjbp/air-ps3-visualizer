#include "ui_trippy.h"

#include <math.h>
#include <string.h>

#define W UI_TRIPPY_W
#define H UI_TRIPPY_H
#define PI_F 3.14159265358979f

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

/* average of two XRGB pixels, per channel, without unpacking */
static inline uint32_t avg2(uint32_t a, uint32_t b)
{
	return (a & b) + (((a ^ b) & 0xfefefeU) >> 1);
}

static uint32_t next_random(ui_trippy *t)
{
	uint32_t x = t->rng;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	t->rng = x;
	return x;
}

void ui_trippy_init(ui_trippy *t)
{
	int i;
	memset(t, 0, sizeof(*t));
	t->rng = 0x2545f491U;
	t->preset = 1;
	for (i = 0; i < 1024; ++i)
		t->sine[i] = (int16_t)(2047.0f * sinf((float)i * 2.0f * PI_F / 1024.0f));
}

/* One-time lookup tables (a few hundred ms on the PPU, done on first use). */
static void build_tables(ui_trippy *t)
{
	int x, y;
	float max_r = sqrtf((float)(W * W + H * H)) * 0.5f;
	for (y = 0; y < H; ++y) {
		for (x = 0; x < W; ++x) {
			int i = y * W + x;
			float dx = (float)x - (float)W * 0.5f + 0.5f;
			float dy = (float)y - (float)H * 0.5f + 0.5f;
			float r = sqrtf(dx * dx + dy * dy);
			float a = atan2f(dy, dx) / (2.0f * PI_F);
			int ang = (int)((a + 1.0f) * 1024.0f) & 1023;
			int wedge = ang & 127; /* 8 mirrored pairs = 8-fold symmetry */
			if (r < 1.0f)
				r = 1.0f;
			t->polar_angle[i] = (uint16_t)ang;
			t->tunnel_depth[i] = (uint16_t)((int)(36000.0f / r) & 1023);
			t->tunnel_shade[i] = (uint8_t)(255.0f * clampf((r - 6.0f) / ((float)H * 0.42f), 0.0f, 1.0f));
			t->kaleido_angle[i] = (uint16_t)((wedge < 64 ? wedge : 127 - wedge) * 16);
			t->polar_radius[i] = (uint16_t)clampf(r * 1023.0f / max_r, 0.0f, 1023.0f);
			t->plasma_radial[i] = (uint16_t)((int)(r * 5.0f) & 1023);
		}
	}
	t->tables_ready = 1;
}

/* Cyclic theme palette: dark -> accent A -> highlight -> accent B -> dark. */
static void build_palette(ui_trippy *t, const ui_theme *theme, float intensity,
	float flash)
{
	uint32_t dark = ui_rgb_scale(theme->bg_bottom, 128);
	uint32_t light_a = ui_rgb_lerp(theme->accent_a, theme->text, 110);
	uint32_t stops[6];
	int i, k = (int)(clampf(intensity + flash * 0.25f, 0.0f, 1.0f) * 256.0f);
	stops[0] = dark;
	stops[1] = theme->accent_a;
	stops[2] = light_a;
	stops[3] = theme->accent_b;
	stops[4] = ui_rgb_lerp(theme->accent_b, dark, 140);
	stops[5] = dark;
	for (i = 0; i < 256; ++i) {
		int seg = i * 5 / 256;
		int f = (i * 5 - seg * 256) ;
		uint32_t c = ui_rgb_lerp(stops[seg], stops[seg + 1], f > 256 ? 256 : f);
		t->palette[i] = ui_rgb_scale(c, k);
	}
}

/* ---- Plasma: interfering sine fields with palette cycling ---- */
static void draw_plasma(ui_trippy *t, uint32_t *dst, const ui_spectrum *s,
	float dt)
{
	static int16_t col[W], row[H], diag[W + H];
	int x, y, i, hue;
	int p1, p2, p3, p4;
	float speed = 0.6f + s->level * 1.6f + s->bass * 1.2f + s->beat * 1.5f;
	t->warp_time += dt * speed;
	p1 = (int)(t->warp_time * 90.0f);
	p2 = (int)(t->warp_time * -70.0f);
	p3 = (int)(t->warp_time * 55.0f);
	p4 = (int)(t->warp_time * -120.0f);
	for (x = 0; x < W; ++x)
		col[x] = t->sine[(x * 3 + p1) & 1023];
	for (y = 0; y < H; ++y)
		row[y] = t->sine[(y * 4 + p2) & 1023];
	for (i = 0; i < W + H; ++i)
		diag[i] = t->sine[(i * 2 + p3) & 1023];
	hue = (int)t->hue;
	for (y = 0, i = 0; y < H; ++y) {
		int ry = row[y];
		for (x = 0; x < W; ++x, ++i) {
			int v = col[x] + ry + diag[x + y] + t->sine[(t->plasma_radial[i] + p4) & 1023];
			dst[i] = t->palette[((v >> 5) + hue) & 255];
		}
	}
}

/* ---- Tunnel: flying through a twisting wormhole ---- */
static void draw_tunnel(ui_trippy *t, uint32_t *dst, const ui_spectrum *s,
	float dt)
{
	const int16_t *S = t->sine;
	int i, rot, travel, hue = (int)t->hue;
	t->travel += dt * (160.0f + s->level * 520.0f + s->bass * 360.0f + s->beat * 600.0f);
	t->spin += dt * (50.0f + s->mids * 200.0f);
	rot = (int)t->spin;
	travel = (int)t->travel;
	for (i = 0; i < UI_TRIPPY_PIXELS; ++i) {
		int d = t->tunnel_depth[i] + travel;
		int a = t->polar_angle[i];
		/* spiral stripes plus rings, both moving down the tunnel */
		int v = S[(a * 4 + d + rot) & 1023] + S[(d * 2 - a * 2 - rot) & 1023];
		dst[i] = ui_rgb_scale(t->palette[((v >> 4) + hue + (t->tunnel_depth[i] >> 3)) & 255],
			t->tunnel_shade[i] + 1);
	}
}

/* ---- Kaleidoscope: an 8-fold mirrored mandala ---- */
static void draw_kaleidoscope(ui_trippy *t, uint32_t *dst, const ui_spectrum *s,
	float dt)
{
	const int16_t *S = t->sine;
	int i, rot, travel, hue = (int)t->hue;
	int zoom = 256 + (int)(s->bass * 140.0f + s->beat * 90.0f);
	t->travel += dt * (70.0f + s->level * 240.0f + s->beat * 280.0f);
	t->spin += dt * (40.0f + s->mids * 160.0f);
	rot = (int)t->spin;
	travel = (int)t->travel;
	for (i = 0; i < UI_TRIPPY_PIXELS; ++i) {
		int a = t->kaleido_angle[i];
		int r = (t->polar_radius[i] * zoom) >> 8;
		int v = S[(a + (r >> 1) + rot) & 1023] + S[(r - travel) & 1023] +
			S[(a * 2 - (r >> 1) - (rot >> 1)) & 1023];
		dst[i] = t->palette[((v >> 5) + hue + (r >> 3)) & 255];
	}
}

/* ---- Feedback: last frame zoomed, rotated and faded, plus the waveform ---- */
static void draw_feedback(ui_trippy *t, uint32_t *dst, const uint32_t *prev,
	const ui_spectrum *s, float dt)
{
	ui_canvas c;
	static int32_t warp_x[H], warp_y[W];
	/* Rates are per 1/60 s and scaled by dt, so it looks the same at 30 fps. */
	float steps = dt * 60.0f;
	/* mostly expanding outward (fills the screen with trails); some presets
	 * pull gently inward for variety */
	float zoom_in = (t->preset & 3) == 2 ? -0.5f : 1.0f;
	float zoom = 1.0f + zoom_in * steps * (0.010f + s->bass * 0.025f + s->beat * 0.03f);
	float angle = (t->preset & 2 ? 1.0f : -1.0f) * steps * (0.004f + s->mids * 0.025f);
	float cx = (float)W * 0.5f + 18.0f * sinf(t->warp_time * 0.37f);
	float cy = (float)H * 0.5f + 12.0f * cosf(t->warp_time * 0.29f);
	/* sampling transform: src = c + R(angle) / zoom * (p - c), 16.16 fixed */
	float ca = cosf(angle) / zoom, sa = sinf(angle) / zoom;
	int32_t m00 = (int32_t)(ca * 65536.0f);
	int32_t m10 = (int32_t)(sa * 65536.0f);
	/* trails fade and drift in colour: red/blue and green decay at
	 * different, slowly alternating rates */
	float keep = powf(0.985f - (1.0f - s->level) * 0.008f, steps);
	float drift = 0.012f * sinf(t->warp_time * 0.21f);
	int keep_rb = (int)(256.0f * clampf(keep * (1.0f + drift), 0.0f, 0.998f));
	int keep_g = (int)(256.0f * clampf(keep * (1.0f - drift), 0.0f, 0.998f));
	/* MilkDrop-style warp: wavy per-row / per-column displacement */
	float warp_amp = (0.8f + s->mids * 2.5f) * steps * 65536.0f;
	int x, y, i, hue = (int)t->hue;
	t->warp_time += dt;
	t->preset_age += dt;
	if ((t->preset_age > 16.0f) || (t->preset_age > 8.0f && s->beat > 0.95f)) {
		t->preset = (int)(next_random(t) & 7U);
		t->preset_age = 0.0f;
	}
	for (y = 0; y < H; ++y)
		warp_x[y] = (int32_t)(warp_amp * sinf((float)y * 0.045f + t->warp_time * 1.3f));
	for (x = 0; x < W; ++x)
		warp_y[x] = (int32_t)(warp_amp * sinf((float)x * 0.03f - t->warp_time * 1.1f));
	for (y = 0; y < H; ++y) {
		float py = (float)y + 0.5f - cy;
		float px = 0.5f - cx;
		int32_t sx = (int32_t)((cx + ca * px - sa * py) * 65536.0f) + warp_x[y];
		int32_t sy = (int32_t)((cy + sa * px + ca * py) * 65536.0f);
		uint32_t *out = dst + y * W;
		for (x = 0; x < W; ++x, sx += m00, sy += m10) {
			int ix = sx >> 16, iy = (sy + warp_y[x]) >> 16;
			uint32_t v = 0;
			if ((unsigned)ix < (unsigned)(W - 1) && (unsigned)iy < (unsigned)(H - 1)) {
				const uint32_t *p = prev + iy * W + ix;
				v = avg2(p[0], p[W + 1]); /* two-tap blur keeps trails soft */
			}
			out[x] = (((v & 0xff00ffU) * (uint32_t)keep_rb >> 8) & 0xff00ffU) |
				(((v & 0x00ff00U) * (uint32_t)keep_g >> 8) & 0x00ff00U);
		}
	}

	/* the new waveform, drawn into the feedback frame */
	c.px = dst;
	c.w = W;
	c.h = H;
	c.stride = W;
	{
		int shape = (t->preset >> 1) & 3;
		float base_r = (float)H * (0.18f + s->bass * 0.08f);
		float amp = (float)H * 0.20f;
		float px_prev = 0.0f, py_prev = 0.0f;
		const int points = 192;
		for (i = 0; i <= points; ++i) {
			int k = (i % points) * (UI_FFT_SIZE / points);
			float w = 0.5f * (s->wave_left[k] + s->wave_right[k]);
			float px, py;
			uint32_t color = t->palette[(hue + i * 256 / points) & 255];
			if (shape == 1) {
				/* horizontal line */
				px = (float)W * ((float)i / (float)points);
				py = (float)H * 0.5f + w * amp * 1.4f;
			} else {
				/* circle (shape 0/2) or star (3) */
				float a = (float)i * 2.0f * PI_F / (float)points + t->spin * 0.02f;
				float star = shape == 3 ? 0.25f * sinf(a * 5.0f) : 0.0f;
				float r = base_r * (1.0f + star) + w * amp;
				px = cx + cosf(a) * r;
				py = cy + sinf(a) * r;
			}
			if (i > 0)
				ui_line_add(&c, px_prev, py_prev, px, py, 2.2f + s->beat * 2.0f, color, 255);
			px_prev = px;
			py_prev = py;
		}
		/* outer ring of spectrum sparks */
		for (i = 0; i < UI_BANDS; i += 2) {
			float a = (float)i * 2.0f * PI_F / (float)UI_BANDS - t->spin * 0.03f;
			float r = base_r * 1.6f + s->bands[i] * (float)H * 0.18f;
			if (s->bands[i] > 0.25f)
				ui_glow_add(&c, cx + cosf(a) * r, cy + sinf(a) * r, 2.0f + s->bands[i] * 4.0f,
					t->palette[(hue + 64 + i * 3) & 255], 120);
		}
		t->spin += dt * 20.0f;
		if (s->beat > 0.9f)
			ui_glow_add(&c, cx, cy, base_r * 0.9f, t->palette[(hue + 128) & 255], 90);
	}
}

/* Expands one 640-pixel row to 1280 with linear interpolation. */
static void expand_row(uint32_t *o, const uint32_t *s)
{
	int x;
	for (x = 0; x < W - 1; ++x) {
		uint32_t a = s[x];
		o[2 * x] = a;
		o[2 * x + 1] = avg2(a, s[x + 1]);
	}
	o[2 * W - 2] = s[W - 1];
	o[2 * W - 1] = s[W - 1];
}

/* Brightness kept on a source row behind the info text (256 = untouched). */
static int scrim_keep(int oy, int scrim_top, int scrim_ramp, float amount)
{
	float f;
	if (amount <= 0.0f || oy < scrim_top)
		return 256;
	f = clampf((float)(oy - scrim_top) / (float)scrim_ramp, 0.0f, 1.0f);
	f = f * f * (3.0f - 2.0f * f);
	return 256 - (int)(f * amount * 150.0f);
}

static const uint32_t *scrimmed_row(const uint32_t *src, int y, uint32_t *buf,
	int scrim_top, int scrim_ramp, float amount)
{
	int keep = scrim_keep(2 * y, scrim_top, scrim_ramp, amount), x;
	const uint32_t *s = src + y * W;
	if (keep >= 256)
		return s;
	for (x = 0; x < W; ++x)
		buf[x] = ui_rgb_scale(s[x], keep);
	return buf;
}

/* Doubles the 640x360 effect into the 1280x720 frame with linear
 * interpolation (the in-between row is averaged at source resolution, which
 * keeps the work per output pixel to a store or an average), darkening the
 * rows behind the info text. */
static void upscale(const uint32_t *src, ui_canvas *out, int scrim_top,
	int scrim_ramp, float scrim_amount)
{
	static uint32_t buf[2][W], mid[W];
	const uint32_t *cur, *next;
	int x, y, which = 0;
	cur = scrimmed_row(src, 0, buf[which], scrim_top, scrim_ramp, scrim_amount);
	for (y = 0; y < H; ++y) {
		uint32_t *o = out->px + (size_t)(2 * y) * (size_t)out->stride;
		if (y + 1 < H) {
			which ^= 1;
			next = scrimmed_row(src, y + 1, buf[which], scrim_top, scrim_ramp,
				scrim_amount);
			for (x = 0; x < W; ++x)
				mid[x] = avg2(cur[x], next[x]);
		} else {
			next = cur;
			memcpy(mid, cur, sizeof(mid));
		}
		expand_row(o, cur);
		expand_row(o + out->stride, mid);
		cur = next;
	}
}

void ui_trippy_draw(ui_trippy *t, ui_canvas *out, int mode,
	const ui_theme *theme, const ui_spectrum *s, float intensity,
	int scrim_top, int scrim_ramp, float scrim_amount, float dt)
{
	uint32_t *dst;
	if (out->w != 2 * W || out->h != 2 * H)
		return;
	if (!t->tables_ready)
		build_tables(t);
	/* colours drift slowly and jump a little on beats */
	t->hue += dt * (10.0f + s->treble * 30.0f) + s->beat * s->beat * 6.0f;
	while (t->hue >= 256.0f)
		t->hue -= 256.0f;
	build_palette(t, theme, intensity, s->beat);
	if (mode == UI_TRIPPY_FEEDBACK) {
		const uint32_t *prev = t->frames[t->current];
		t->current ^= 1;
		dst = t->frames[t->current];
		draw_feedback(t, dst, prev, s, dt);
	} else {
		dst = t->frames[t->current];
		if (mode == UI_TRIPPY_TUNNEL)
			draw_tunnel(t, dst, s, dt);
		else if (mode == UI_TRIPPY_KALEIDOSCOPE)
			draw_kaleidoscope(t, dst, s, dt);
		else
			draw_plasma(t, dst, s, dt);
	}
	upscale(dst, out, scrim_top, scrim_ramp, scrim_amount);
}

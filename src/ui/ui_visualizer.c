#include "ui_visualizer.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979f

static const char *const mode_names[UI_VIS_COUNT] = {
	"Spectrum", "Ribbons", "Halo", "Scope", "Nebula"
};

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static uint32_t next_random(ui_visualizer *v)
{
	uint32_t x = v->rng;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	v->rng = x;
	return x;
}

static float random_unit(ui_visualizer *v)
{
	return (float)(next_random(v) & 0xffffffU) / 16777216.0f;
}

const char *ui_visualizer_name(int mode)
{
	if (mode < 0 || mode >= UI_VIS_COUNT)
		return "";
	return mode_names[mode];
}

static void respawn(ui_visualizer *v, ui_particle *p, int far)
{
	float angle = random_unit(v) * 2.0f * PI_F;
	float radius = 0.08f + random_unit(v) * 0.92f;
	p->x = cosf(angle) * radius;
	p->y = sinf(angle) * radius;
	p->z = far ? 1.0f : 0.15f + random_unit(v) * 0.85f;
	p->hue = random_unit(v);
	p->has_prev = 0;
}

void ui_visualizer_init(ui_visualizer *v)
{
	int i;
	memset(v, 0, sizeof(*v));
	v->rng = 0x9e3779b9U;
	for (i = 0; i < UI_NEBULA_PARTICLES; ++i)
		respawn(v, &v->particles[i], 0);
	for (i = 0; i < UI_SCOPE_MAX_WIDTH; ++i)
		v->scope_edge[i] = sinf((float)i / (float)UI_SCOPE_MAX_WIDTH * PI_F);
}

static int scaled(float alpha, const ui_vis_layout *layout)
{
	return (int)clampf(alpha * layout->intensity, 0.0f, 255.0f);
}

/* ---- Spectrum: mirrored rounded bars with peak caps ---- */
static void draw_spectrum(ui_canvas *fx, const ui_theme *theme,
	const ui_spectrum *s, const ui_vis_layout *l)
{
	const int bars = UI_BANDS;
	float pitch = l->width / (float)bars;
	float bar_w = pitch * 0.62f;
	float left = l->center_x - l->width * 0.5f + (pitch - bar_w) * 0.5f;
	float base = l->center_y + l->height * 0.32f;
	float max_h = l->height * 0.78f;
	int i;
	ui_glow_ellipse_add(fx, l->center_x, base, l->width * 0.42f, l->height * 0.14f,
		theme->accent_a, scaled(26.0f + s->bass * 50.0f, l));
	for (i = 0; i < bars; ++i) {
		float v = s->bands[i];
		float h = 4.0f + v * max_h;
		float x = left + (float)i * pitch;
		uint32_t top = ui_theme_ramp(theme, i * 256 / (bars - 1));
		uint32_t bottom = ui_rgb_lerp(top, theme->accent_a, 140);
		float peak_y = base - 4.0f - s->peaks[i] * max_h;
		/* solid core and a faint floor reflection */
		ui_round_rect_add(fx, x, base - h, bar_w, h, bar_w * 0.5f, top, bottom,
			scaled(150.0f + v * 105.0f, l));
		ui_round_rect_add(fx, x, base + 6.0f, bar_w, h * 0.32f, bar_w * 0.5f,
			bottom, 0x000000, scaled(46.0f, l));
		ui_round_rect_add(fx, x, peak_y - 3.0f, bar_w, 3.0f, 1.5f, theme->text,
			theme->text, scaled(90.0f + s->peaks[i] * 120.0f, l));
	}
}

/* ---- Ribbons: XMB-style translucent waves driven by the music ---- */
static void draw_ribbons(ui_visualizer *vis, ui_canvas *fx,
	const ui_theme *theme, const ui_spectrum *s, const ui_vis_layout *l,
	float time, float dt)
{
	static const float freq[5] = { 1.15f, 1.6f, 0.85f, 2.1f, 1.35f };
	static const float speed[5] = { 0.35f, -0.27f, 0.21f, 0.44f, -0.18f };
	static const float offset[5] = { 0.0f, 1.7f, 3.1f, 4.4f, 5.6f };
	float energy[5];
	int r, x;
	int x0 = 0, x1 = fx->w;
	energy[0] = s->bass;
	energy[1] = s->mids;
	energy[2] = s->bass * 0.6f + s->mids * 0.4f;
	energy[3] = s->treble;
	energy[4] = s->level;
	vis->ribbon_phase += dt * (0.6f + s->level * 1.4f + s->beat * 1.5f);
	for (r = 0; r < 5; ++r) {
		uint32_t color = ui_theme_ramp(theme, r * 64);
		float amp = l->height * (0.07f + energy[r] * 0.22f + s->beat * 0.04f);
		float thickness = l->height * (0.08f + 0.03f * (float)(r % 3));
		float prev_y = 0.0f;
		float phase = vis->ribbon_phase * speed[r] * 2.0f + offset[r];
		for (x = x0; x < x1; ++x) {
			float u = (float)x / (float)fx->w;
			float edge = sinf(u * PI_F);
			float y = l->center_y + l->height * 0.06f * (float)(r - 2) +
				amp * sinf(u * freq[r] * 2.0f * PI_F + phase) +
				amp * 0.35f * sinf(u * freq[r] * 5.3f * PI_F - phase * 1.7f + time * 0.3f);
			int a_line = scaled((70.0f + energy[r] * 150.0f) * (0.35f + 0.65f * edge), l);
			int a_sheet = scaled((16.0f + energy[r] * 26.0f) * (0.4f + 0.6f * edge), l);
			if (x == x0)
				prev_y = y;
			if (r < 3)
				ui_vspan_add(fx, x, y, y + thickness, color, a_sheet, 0);
			ui_vspan_add(fx, x, prev_y - 0.9f, y + 0.9f, color, a_line, a_line);
			prev_y = y;
		}
	}
}

/* ---- Halo: radial spectrum ring, mirrored left/right, bass at the bottom ---- */
static void draw_halo(ui_visualizer *vis, ui_canvas *fx, const ui_theme *theme,
	const ui_spectrum *s, const ui_vis_layout *l, float dt)
{
	const int spokes = 120;
	float radius = l->height * (0.26f + s->bass * 0.05f + s->beat * 0.025f);
	float max_len = l->height * 0.34f;
	float cx = l->center_x, cy = l->center_y;
	int i;
	vis->spin += dt * (0.15f + s->level * 0.6f);
	ui_glow_add(fx, cx, cy, radius * 1.55f, theme->accent_b,
		scaled(12.0f + s->bass * 26.0f, l));
	ui_glow_add(fx, cx, cy, radius * 1.02f, theme->accent_a,
		scaled(14.0f + s->beat * 46.0f, l));
	for (i = 0; i < spokes; ++i) {
		int half = spokes / 2;
		int k = i < half ? i : spokes - 1 - i;
		int band = k * (UI_BANDS - 1) / (half - 1);
		float v = s->bands[band];
		float angle = PI_F * 0.5f + ((float)i + 0.5f) * 2.0f * PI_F / (float)spokes;
		float ca = cosf(angle), sa = sinf(angle);
		float len = 3.0f + v * max_len;
		uint32_t color = ui_theme_ramp(theme, band * 256 / (UI_BANDS - 1));
		float r0 = radius + 6.0f;
		ui_line_add(fx, cx + ca * r0, cy + sa * r0, cx + ca * (r0 + len),
			cy + sa * (r0 + len), 3.2f, color, scaled(140.0f + v * 115.0f, l));
	}
	ui_ring_add(fx, cx, cy, radius - 8.0f, 1.6f, theme->text,
		scaled(60.0f + s->beat * 120.0f, l));
	/* a small orbiting highlight on the inner ring */
	ui_glow_add(fx, cx + cosf(vis->spin) * (radius - 8.0f),
		cy + sinf(vis->spin) * (radius - 8.0f), 18.0f, theme->text,
		scaled(90.0f + s->level * 100.0f, l));
}

/* ---- Scope: triggered stereo waveform; the last few frames are redrawn
 * fainter as phosphor-style echoes (cheaper than fading a full layer) ---- */
static void draw_scope(ui_visualizer *vis, ui_canvas *fx, const ui_theme *theme,
	const ui_spectrum *s, const ui_vis_layout *l)
{
	const int span = UI_FFT_SIZE * 3 / 4;
	int start = 0, i, x, h, ch;
	int x0 = (int)(l->center_x - l->width * 0.5f);
	int width = (int)l->width;
	float amp = l->height * 0.42f;
	if (width > UI_SCOPE_MAX_WIDTH)
		width = UI_SCOPE_MAX_WIDTH;
	/* Trigger on a rising zero crossing for a steady picture. */
	for (i = 1; i < UI_FFT_SIZE - span; ++i) {
		float a = s->wave_left[i - 1] + s->wave_right[i - 1];
		float b = s->wave_left[i] + s->wave_right[i];
		if (a < 0.0f && b >= 0.0f) {
			start = i;
			break;
		}
	}
	/* shift history and compute the new trace */
	memmove(&vis->scope_y[1], &vis->scope_y[0],
		sizeof(vis->scope_y[0]) * (UI_SCOPE_HISTORY - 1));
	for (ch = 0; ch < 2; ++ch) {
		const float *wave = ch == 0 ? s->wave_left : s->wave_right;
		for (x = 0; x < width; ++x) {
			float pos = (float)start + (float)x * (float)(span - 1) / (float)(width - 1);
			int k = (int)pos;
			float f = pos - (float)k;
			float sample = wave[k] + (wave[k + 1] - wave[k]) * f;
			float edge = vis->scope_edge[x];
			vis->scope_y[0][ch][x] = l->center_y +
				clampf(sample * 1.6f, -1.0f, 1.0f) * amp * edge;
		}
	}
	if (vis->scope_frames < UI_SCOPE_HISTORY)
		++vis->scope_frames;
	for (h = vis->scope_frames - 1; h >= 0; --h) {
		static const float echo[UI_SCOPE_HISTORY] = { 1.0f, 0.42f, 0.2f, 0.09f };
		for (ch = 0; ch < 2; ++ch) {
			const float *y = vis->scope_y[h][ch];
			uint32_t color = ch == 0 ? theme->accent_a : theme->accent_b;
			int core = scaled(220.0f * echo[h], l);
			int glow = scaled(30.0f * echo[h], l);
			for (x = 1; x < width; ++x) {
				if (h == 0)
					ui_vspan_add(fx, x0 + x, y[x - 1] - 3.5f, y[x] + 3.5f, color, glow, glow);
				ui_vspan_add(fx, x0 + x, y[x - 1] - 1.0f, y[x] + 1.0f, color, core, core);
			}
		}
	}
}

/* ---- Nebula: a starfield that surges with the beat. Each star draws its
 * own motion streak (tail length follows speed), so no trail layer. ---- */
static void project(const ui_vis_layout *l, float rot_c, float rot_s,
	const ui_particle *p, float z, float *sx, float *sy)
{
	float focal = l->height * 0.55f;
	float rx = p->x * rot_c - p->y * rot_s;
	float ry = p->x * rot_s + p->y * rot_c;
	*sx = l->center_x + rx / z * focal;
	*sy = l->center_y + ry / z * focal;
}

static void draw_nebula(ui_visualizer *vis, ui_canvas *fx, const ui_theme *theme,
	const ui_spectrum *s, const ui_vis_layout *l, float dt)
{
	float speed = 0.10f + s->level * 0.55f + s->bass * 0.35f + s->beat * 0.9f;
	float tail = 0.03f + speed * 0.22f;
	float rot_c, rot_s;
	int i;
	vis->spin += dt * (0.04f + s->mids * 0.2f);
	rot_c = cosf(vis->spin);
	rot_s = sinf(vis->spin);
	ui_glow_add(fx, l->center_x, l->center_y, l->height * (0.22f + s->bass * 0.18f),
		theme->accent_b, scaled(8.0f + s->bass * 26.0f + s->beat * 30.0f, l));
	for (i = 0; i < UI_NEBULA_PARTICLES; ++i) {
		ui_particle *p = &vis->particles[i];
		float sx, sy, tx, ty, depth, size;
		uint32_t color;
		p->z -= speed * dt * (0.6f + p->hue * 0.8f);
		if (p->z <= 0.03f) {
			respawn(vis, p, 1);
			continue;
		}
		project(l, rot_c, rot_s, p, p->z, &sx, &sy);
		if (sx < -50.0f || sx > (float)fx->w + 50.0f || sy < -50.0f ||
			sy > (float)fx->h + 50.0f) {
			respawn(vis, p, 1);
			continue;
		}
		project(l, rot_c, rot_s, p, p->z + tail * (0.6f + p->hue * 0.8f), &tx, &ty);
		depth = clampf(1.0f - p->z, 0.0f, 1.0f);
		size = 1.0f + depth * depth * 3.0f;
		color = ui_theme_ramp(theme, (int)(p->hue * 256.0f));
		ui_line_add(fx, tx, ty, sx, sy, size, color, scaled(25.0f + depth * 150.0f, l));
		ui_glow_add(fx, sx, sy, size * 2.2f, color, scaled(60.0f + depth * 150.0f, l));
	}
}

void ui_visualizer_draw(ui_visualizer *v, ui_canvas *fx, int mode,
	const ui_theme *theme, const ui_spectrum *s, const ui_vis_layout *layout,
	float time, float dt)
{
	switch (mode) {
	case UI_VIS_RIBBONS:
		draw_ribbons(v, fx, theme, s, layout, time, dt);
		break;
	case UI_VIS_HALO:
		draw_halo(v, fx, theme, s, layout, dt);
		break;
	case UI_VIS_SCOPE:
		draw_scope(v, fx, theme, s, layout);
		break;
	case UI_VIS_NEBULA:
		draw_nebula(v, fx, theme, s, layout, dt);
		break;
	case UI_VIS_SPECTRUM:
	default:
		draw_spectrum(fx, theme, s, layout);
		break;
	}
}

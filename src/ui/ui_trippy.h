#ifndef PS3_AIRPLAY_UI_TRIPPY_H
#define PS3_AIRPLAY_UI_TRIPPY_H

#include <stdint.h>

#include "ui_canvas.h"
#include "ui_spectrum.h"
#include "ui_theme.h"

/* Full-screen abstract effects. They are computed at quarter resolution
 * (640x360) with table lookups and integer math, then smoothly doubled into
 * the 1280x720 frame (the RSX scales that to 1080p). Soft, dreamy pixels
 * suit these effects and keep them affordable on the PS3's PPU. */
#define UI_TRIPPY_W 640
#define UI_TRIPPY_H 360
#define UI_TRIPPY_PIXELS (UI_TRIPPY_W * UI_TRIPPY_H)

typedef enum {
	UI_TRIPPY_FEEDBACK = 0,
	UI_TRIPPY_TUNNEL,
	UI_TRIPPY_KALEIDOSCOPE,
	UI_TRIPPY_PLASMA,
	UI_TRIPPY_COUNT
} ui_trippy_mode;

typedef struct {
	/* feedback frames (previous / current) */
	uint32_t frames[2][UI_TRIPPY_PIXELS];
	int current;
	/* per-pixel lookup tables, built the first time a mode is used */
	/* 10-bit coordinates (0..1023) index the sine table directly, so the
	 * patterns stay smooth even far from the centre. */
	uint16_t polar_angle[UI_TRIPPY_PIXELS];   /* around the centre */
	uint16_t tunnel_depth[UI_TRIPPY_PIXELS];  /* 1/r, wraps */
	uint8_t tunnel_shade[UI_TRIPPY_PIXELS];   /* darker towards the vanishing point */
	uint16_t kaleido_angle[UI_TRIPPY_PIXELS]; /* folded into one mirrored wedge */
	uint16_t polar_radius[UI_TRIPPY_PIXELS];
	uint16_t plasma_radial[UI_TRIPPY_PIXELS];
	int16_t sine[1024];                      /* sin * 2047 */
	uint32_t palette[256];
	int tables_ready;

	/* animation state */
	float travel;
	float spin;
	float hue;
	float warp_time;
	int preset;
	float preset_age;
	uint32_t rng;
} ui_trippy;

void ui_trippy_init(ui_trippy *t);
/* Renders the effect and writes it (doubled) over the whole frame. Rows at
 * and below scrim_top get darkened by up to scrim_amount (0..1) so the info
 * text stays readable. intensity dims the effect (0..1). */
void ui_trippy_draw(ui_trippy *t, ui_canvas *out, int mode,
	const ui_theme *theme, const ui_spectrum *s, float intensity,
	int scrim_top, int scrim_ramp, float scrim_amount, float dt);

#endif

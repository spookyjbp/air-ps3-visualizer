#ifndef PS3_AIRPLAY_UI_VISUALIZER_H
#define PS3_AIRPLAY_UI_VISUALIZER_H

#include <stdint.h>

#include "ui_canvas.h"
#include "ui_spectrum.h"
#include "ui_theme.h"

typedef enum {
	UI_VIS_SPECTRUM = 0,
	UI_VIS_RIBBONS,
	UI_VIS_HALO,
	UI_VIS_SCOPE,
	UI_VIS_NEBULA,
	UI_VIS_COUNT
} ui_vis_mode;

#define UI_NEBULA_PARTICLES 260

typedef struct {
	float x, y, z;      /* z: depth 0..1, smaller is closer to the viewer */
	float px, py;       /* last projected position */
	float hue;
	int has_prev;
} ui_particle;

typedef struct {
	ui_particle particles[UI_NEBULA_PARTICLES];
	uint32_t rng;
	float spin;
	float travel;
	float ribbon_phase;
} ui_visualizer;

typedef struct {
	float center_x;
	float center_y;
	float width;     /* usable width for horizontal modes */
	float height;    /* usable height for the main motion */
	float intensity; /* 0..1 overall brightness (dimmed while idle) */
} ui_vis_layout;

const char *ui_visualizer_name(int mode);
void ui_visualizer_init(ui_visualizer *v);
/* How much of the previous frame's light layer to keep (0..256). */
int ui_visualizer_trail_keep(int mode);
void ui_visualizer_draw(ui_visualizer *v, ui_canvas *fx, int mode,
	const ui_theme *theme, const ui_spectrum *s, const ui_vis_layout *layout,
	float time, float dt);

#endif

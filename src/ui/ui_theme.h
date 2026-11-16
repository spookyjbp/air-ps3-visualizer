#ifndef PS3_AIRPLAY_UI_THEME_H
#define PS3_AIRPLAY_UI_THEME_H

#include <stdint.h>

typedef struct {
	const char *name;
	uint32_t bg_top;
	uint32_t bg_bottom;
	uint32_t bg_glow;   /* soft light behind the visualizer */
	uint32_t accent_a;  /* low frequencies / primary */
	uint32_t accent_b;  /* high frequencies / secondary */
	uint32_t text;
} ui_theme;

#define UI_THEME_COUNT 6

const ui_theme *ui_theme_get(int index);
/* Palette lookup along accent_a -> accent_b, t in 0..256. */
uint32_t ui_theme_ramp(const ui_theme *theme, int t);

#endif

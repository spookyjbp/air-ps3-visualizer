#include "ui_theme.h"
#include "ui_canvas.h"

static const ui_theme themes[UI_THEME_COUNT] = {
	{ "Aurora", 0x060a14, 0x0b1222, 0x0d2a33, 0x3ee6c1, 0x8a5cff, 0xeef3ff },
	{ "Sunset", 0x10070f, 0x1c0a14, 0x3a1020, 0xffa24a, 0xff3d8b, 0xfff1ea },
	{ "Silver", 0x090b10, 0x161b26, 0x1c2433, 0xdfe7f3, 0x7f9cc4, 0xf4f7fb },
	{ "Ocean", 0x020c15, 0x05192a, 0x07304a, 0x2fb8ff, 0x2affd5, 0xe8f7ff },
	{ "Neon", 0x08050f, 0x12071c, 0x2a0a3a, 0xff2ee6, 0x29f0ff, 0xfbeeff },
	{ "Ember", 0x0c0705, 0x170b06, 0x33150a, 0xffd36b, 0xff4e1f, 0xfff4e6 }
};

const ui_theme *ui_theme_get(int index)
{
	if (index < 0)
		index = 0;
	return &themes[index % UI_THEME_COUNT];
}

uint32_t ui_theme_ramp(const ui_theme *theme, int t)
{
	if (t < 0) t = 0;
	if (t > 256) t = 256;
	return ui_rgb_lerp(theme->accent_a, theme->accent_b, t);
}

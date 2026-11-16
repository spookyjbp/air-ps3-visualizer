#ifndef PS3_AIRPLAY_UI_FONT_H
#define PS3_AIRPLAY_UI_FONT_H

#include <stdint.h>

/* One pre-rasterized glyph. Alpha is packed two 4-bit pixels per byte. */
typedef struct {
	int16_t advance_q2; /* advance width in quarter pixels */
	int16_t x;          /* left bearing from the pen position */
	int16_t y;          /* top of bitmap relative to the baseline (negative is up) */
	int16_t w;
	int16_t h;
	uint32_t offset;    /* byte offset into the face's alpha data */
} ui_glyph;

typedef struct {
	int16_t size;
	int16_t ascent;
	int16_t descent;
	uint16_t count;
	const ui_glyph *glyphs;
	const uint8_t *alpha;
} ui_font;

typedef enum {
	UI_FONT_SMALL = 0,
	UI_FONT_BODY = 1,
	UI_FONT_TITLE = 2
} ui_font_id;

const ui_font *ui_font_get(ui_font_id id);

/* Maps a Unicode code point to a glyph index, folding common typographic
 * punctuation to Latin-1 equivalents. Returns the index of '?' if unknown. */
int ui_font_glyph_index(uint32_t codepoint);

/* Decodes one UTF-8 sequence. Invalid bytes decode as U+FFFD and advance 1. */
uint32_t ui_utf8_next(const char **cursor);

#endif

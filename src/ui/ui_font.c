#include "ui_font.h"
#include "ui_font_data.h"

const ui_font *ui_font_get(ui_font_id id)
{
	switch (id) {
	case UI_FONT_TITLE:
		return &ui_font_title;
	case UI_FONT_BODY:
		return &ui_font_body;
	case UI_FONT_SMALL:
	default:
		return &ui_font_small;
	}
}

int ui_font_glyph_index(uint32_t cp)
{
	switch (cp) {
	case 0x2018: case 0x2019: case 0x201a: case 0x2032: cp = '\''; break;
	case 0x201c: case 0x201d: case 0x201e: case 0x2033: cp = '"'; break;
	case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2212: cp = '-'; break;
	case 0x2014: case 0x2015: cp = '-'; break;
	case 0x2022: case 0x2027: case 0x30fb: cp = 0xb7; break;
	case 0x2026: cp = '.'; break; /* single glyph; ui_text_fit uses "..." */
	case 0x00a0: case 0x2002: case 0x2003: case 0x2009: case 0x202f: cp = ' '; break;
	case 0x2122: cp = 'T'; break;
	case 0x0152: return ui_font_glyph_index('O');
	case 0x0153: return ui_font_glyph_index('o');
	case 0x0160: cp = 'S'; break;
	case 0x0161: cp = 's'; break;
	case 0x017d: cp = 'Z'; break;
	case 0x017e: cp = 'z'; break;
	case 0x0178: cp = 'Y'; break;
	default: break;
	}
	if (cp >= 32 && cp <= 126)
		return (int)(cp - 32);
	if (cp >= 160 && cp <= 255)
		return (int)(95 + cp - 160);
	return '?' - 32;
}

uint32_t ui_utf8_next(const char **cursor)
{
	const unsigned char *s = (const unsigned char *)*cursor;
	uint32_t cp;
	int extra, i;
	if (s[0] < 0x80) {
		*cursor += 1;
		return s[0];
	}
	if ((s[0] & 0xe0) == 0xc0) {
		cp = s[0] & 0x1f;
		extra = 1;
	} else if ((s[0] & 0xf0) == 0xe0) {
		cp = s[0] & 0x0f;
		extra = 2;
	} else if ((s[0] & 0xf8) == 0xf0) {
		cp = s[0] & 0x07;
		extra = 3;
	} else {
		*cursor += 1;
		return 0xfffd;
	}
	for (i = 1; i <= extra; ++i) {
		if ((s[i] & 0xc0) != 0x80) {
			*cursor += 1;
			return 0xfffd;
		}
		cp = (cp << 6) | (s[i] & 0x3f);
	}
	*cursor += 1 + extra;
	return cp;
}

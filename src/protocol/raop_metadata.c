#include "raop_metadata.h"

#include <string.h>

#define DMAP_MAX_DEPTH 4

void raop_metadata_clear(raop_metadata *metadata)
{
	uint32_t text_updates, progress_updates;
	if (metadata == NULL)
		return;
	text_updates = metadata->text_updates;
	progress_updates = metadata->progress_updates;
	memset(metadata, 0, sizeof(*metadata));
	/* Keep counters monotonic so observers notice the reset. */
	metadata->text_updates = text_updates + 1U;
	metadata->progress_updates = progress_updates + 1U;
}

static size_t utf8_sequence(const uint8_t *s, size_t remaining)
{
	size_t extra, i;
	uint32_t cp;
	if (s[0] < 0x80)
		return 1;
	if ((s[0] & 0xe0) == 0xc0) {
		extra = 1;
		cp = s[0] & 0x1fU;
	} else if ((s[0] & 0xf0) == 0xe0) {
		extra = 2;
		cp = s[0] & 0x0fU;
	} else if ((s[0] & 0xf8) == 0xf0) {
		extra = 3;
		cp = s[0] & 0x07U;
	} else {
		return 0;
	}
	if (extra >= remaining)
		return 0;
	for (i = 1; i <= extra; ++i) {
		if ((s[i] & 0xc0) != 0x80)
			return 0;
		cp = (cp << 6) | (s[i] & 0x3fU);
	}
	/* reject overlong forms, surrogates and out-of-range values */
	if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
		(extra == 3 && cp < 0x10000) || cp > 0x10ffff ||
		(cp >= 0xd800 && cp <= 0xdfff))
		return 0;
	return extra + 1;
}

void raop_metadata_copy_text(char *out, size_t capacity, const uint8_t *in,
	size_t length)
{
	size_t o = 0, i = 0;
	int pending_space = 0;
	if (capacity == 0)
		return;
	while (i < length) {
		size_t n = utf8_sequence(in + i, length - i);
		if (n == 1 && (in[i] < 0x20 || in[i] == 0x7f || in[i] == ' ')) {
			pending_space = o > 0;
			++i;
			continue;
		}
		if (pending_space) {
			if (o + 1 >= capacity)
				break;
			out[o++] = ' ';
			pending_space = 0;
		}
		if (n == 0) {
			if (o + 1 >= capacity)
				break;
			out[o++] = '?';
			++i;
			continue;
		}
		if (o + n >= capacity)
			break;
		memcpy(out + o, in + i, n);
		o += n;
		i += n;
	}
	out[o] = '\0';
}

static uint32_t read_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
		((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int walk_dmap(raop_metadata *metadata, const uint8_t *data,
	size_t length, int depth, int *found)
{
	size_t offset = 0;
	if (depth > DMAP_MAX_DEPTH)
		return 0;
	while (offset < length) {
		const uint8_t *tag;
		uint32_t item_length;
		if (length - offset < 8)
			return 0;
		tag = data + offset;
		item_length = read_be32(data + offset + 4);
		offset += 8;
		if (item_length > length - offset)
			return 0;
		if (memcmp(tag, "mlit", 4) == 0 || memcmp(tag, "mlcl", 4) == 0) {
			if (!walk_dmap(metadata, data + offset, item_length, depth + 1, found))
				return 0;
		} else if (memcmp(tag, "minm", 4) == 0) {
			raop_metadata_copy_text(metadata->title, sizeof(metadata->title),
				data + offset, item_length);
			*found = 1;
		} else if (memcmp(tag, "asar", 4) == 0) {
			raop_metadata_copy_text(metadata->artist, sizeof(metadata->artist),
				data + offset, item_length);
			*found = 1;
		} else if (memcmp(tag, "asal", 4) == 0) {
			raop_metadata_copy_text(metadata->album, sizeof(metadata->album),
				data + offset, item_length);
			*found = 1;
		}
		offset += item_length;
	}
	return 1;
}

int raop_metadata_parse_dmap(raop_metadata *metadata, const uint8_t *body,
	size_t length)
{
	raop_metadata parsed;
	int found = 0;
	if (metadata == NULL || (body == NULL && length > 0))
		return 0;
	/* Parse into a copy with cleared text so one track's fields never
	 * leak into the next; commit only well-formed bodies. */
	parsed = *metadata;
	parsed.title[0] = parsed.artist[0] = parsed.album[0] = '\0';
	if (!walk_dmap(&parsed, body, length, 0, &found))
		return 0;
	if (found) {
		memcpy(metadata->title, parsed.title, sizeof(metadata->title));
		memcpy(metadata->artist, parsed.artist, sizeof(metadata->artist));
		memcpy(metadata->album, parsed.album, sizeof(metadata->album));
		++metadata->text_updates;
	}
	return 1;
}

static int parse_u32(const char *s, size_t length, size_t *index, uint32_t *out)
{
	uint64_t value = 0;
	size_t start = *index;
	while (*index < length && s[*index] >= '0' && s[*index] <= '9') {
		value = value * 10U + (uint64_t)(s[*index] - '0');
		if (value > 0xffffffffULL)
			return 0;
		++*index;
	}
	if (*index == start)
		return 0;
	*out = (uint32_t)value;
	return 1;
}

int raop_metadata_parse_progress(raop_metadata *metadata, const char *body,
	size_t length)
{
	size_t line = 0;
	if (metadata == NULL || body == NULL)
		return 0;
	while (line < length) {
		size_t end = line, i;
		uint32_t values[3];
		while (end < length && body[end] != '\r' && body[end] != '\n')
			++end;
		i = line;
		while (i < end && (body[i] == ' ' || body[i] == '\t'))
			++i;
		if (end - i >= 9 && (body[i] == 'p' || body[i] == 'P') &&
			memcmp(body + i + 1, "rogress", 7) == 0) {
			int k, ok = 1;
			i += 8;
			while (i < end && (body[i] == ' ' || body[i] == '\t'))
				++i;
			if (i >= end || body[i] != ':')
				ok = 0;
			else
				++i;
			for (k = 0; ok && k < 3; ++k) {
				while (i < end && (body[i] == ' ' || body[i] == '\t'))
					++i;
				if (!parse_u32(body, end, &i, &values[k]))
					ok = 0;
				else if (k < 2 && (i >= end || body[i++] != '/'))
					ok = 0;
			}
			if (ok) {
				metadata->progress_start = values[0];
				metadata->progress_current = values[1];
				metadata->progress_end = values[2];
				metadata->has_progress = 1;
				++metadata->progress_updates;
				return 1;
			}
		}
		line = end;
		while (line < length && (body[line] == '\r' || body[line] == '\n'))
			++line;
	}
	return 0;
}

#ifndef PS3_AIRPLAY_RAOP_METADATA_H
#define PS3_AIRPLAY_RAOP_METADATA_H

#include <stddef.h>
#include <stdint.h>

/* Now-playing information a sender pushes with SET_PARAMETER when the
 * receiver advertises md=0,2 (text metadata and progress, no artwork). */
#define RAOP_METADATA_TEXT_MAX 128

typedef struct {
	char title[RAOP_METADATA_TEXT_MAX];
	char artist[RAOP_METADATA_TEXT_MAX];
	char album[RAOP_METADATA_TEXT_MAX];
	uint32_t text_updates;
	/* RTP timestamps from "progress: start/current/end" */
	uint32_t progress_start;
	uint32_t progress_current;
	uint32_t progress_end;
	uint8_t has_progress;
	uint32_t progress_updates;
} raop_metadata;

void raop_metadata_clear(raop_metadata *metadata);

/* Parses an application/x-dmap-tagged body (an mlit listing). Recognized
 * fields replace the current text; returns 1 if the body was well formed. */
int raop_metadata_parse_dmap(raop_metadata *metadata, const uint8_t *body,
	size_t length);

/* Looks for a "progress: a/b/c" line in a text/parameters body. Returns 1 if
 * one was found and parsed. */
int raop_metadata_parse_progress(raop_metadata *metadata, const char *body,
	size_t length);

/* Copies UTF-8 text, replacing control characters and malformed sequences
 * with spaces/'?', collapsing whitespace, and truncating on a code-point
 * boundary. Always NUL-terminates when capacity > 0. */
void raop_metadata_copy_text(char *out, size_t capacity, const uint8_t *in,
	size_t length);

#endif

#include "../../src/protocol/raop_metadata.h"
#include "../../src/protocol/raop_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t put_item(uint8_t *out, const char tag[4], const void *data,
	size_t length)
{
	memcpy(out, tag, 4);
	out[4] = (uint8_t)(length >> 24);
	out[5] = (uint8_t)(length >> 16);
	out[6] = (uint8_t)(length >> 8);
	out[7] = (uint8_t)length;
	if (length > 0)
		memcpy(out + 8, data, length);
	return 8 + length;
}

static size_t build_listing(uint8_t *out, const char *title, const char *artist,
	const char *album)
{
	uint8_t inner[1024];
	size_t n = 0;
	static const uint8_t kind = 2;
	n += put_item(inner + n, "mikd", &kind, 1);
	if (title) n += put_item(inner + n, "minm", title, strlen(title));
	if (artist) n += put_item(inner + n, "asar", artist, strlen(artist));
	if (album) n += put_item(inner + n, "asal", album, strlen(album));
	n += put_item(inner + n, "asgn", "Synthwave", 9);
	return put_item(out, "mlit", inner, n);
}

static void test_copy_text(void)
{
	char out[16];
	raop_metadata_copy_text(out, sizeof(out), (const uint8_t *)"  Caf\xc3\xa9\tDel\nMar ", 16);
	assert(strcmp(out, "Caf\xc3\xa9 Del Mar") == 0);
	/* invalid sequences become '?' */
	raop_metadata_copy_text(out, sizeof(out), (const uint8_t *)"A\xff\xc3-B", 5);
	assert(strcmp(out, "A?" "?-B") == 0);
	/* overlong encoding of '/' is rejected */
	raop_metadata_copy_text(out, sizeof(out), (const uint8_t *)"\xc0\xaf", 2);
	assert(strcmp(out, "?" "?") == 0);
	/* truncation never splits a code point */
	raop_metadata_copy_text(out, 6, (const uint8_t *)"abcd\xc3\xa9", 6);
	assert(strcmp(out, "abcd") == 0);
	raop_metadata_copy_text(out, 1, (const uint8_t *)"abc", 3);
	assert(out[0] == '\0');
}

static void test_dmap(void)
{
	raop_metadata m;
	uint8_t body[2048];
	size_t n;
	memset(&m, 0, sizeof(m));
	n = build_listing(body, "Midnight Drive", "Nova Lights", "Neon Coast");
	assert(raop_metadata_parse_dmap(&m, body, n));
	assert(strcmp(m.title, "Midnight Drive") == 0);
	assert(strcmp(m.artist, "Nova Lights") == 0);
	assert(strcmp(m.album, "Neon Coast") == 0);
	assert(m.text_updates == 1);

	/* A new track without an album does not keep the old album. */
	n = build_listing(body, "Second", "Other", NULL);
	assert(raop_metadata_parse_dmap(&m, body, n));
	assert(strcmp(m.title, "Second") == 0 && m.album[0] == '\0');
	assert(m.text_updates == 2);

	/* Truncated bodies are rejected and leave the current text alone. */
	n = build_listing(body, "Broken", "X", "Y");
	assert(!raop_metadata_parse_dmap(&m, body, n - 3));
	assert(!raop_metadata_parse_dmap(&m, body, 5));
	body[7] = 0xff; /* outer length larger than the body */
	assert(!raop_metadata_parse_dmap(&m, body, n));
	assert(strcmp(m.title, "Second") == 0 && m.text_updates == 2);

	/* Excessive nesting is rejected. */
	{
		uint8_t nested[256];
		size_t i, len = put_item(nested + 64, "minm", "x", 1);
		size_t start = 64;
		for (i = 0; i < 6; ++i) {
			start -= 8;
			memcpy(nested + start, "mlit", 4);
			nested[start + 4] = 0; nested[start + 5] = 0;
			nested[start + 6] = (uint8_t)(len >> 8); nested[start + 7] = (uint8_t)len;
			len += 8;
		}
		assert(!raop_metadata_parse_dmap(&m, nested + start, len));
	}
	/* Empty body is well formed and changes nothing. */
	assert(raop_metadata_parse_dmap(&m, body, 0));
	assert(m.text_updates == 2);

	/* Oversized strings are truncated, not overflowed. */
	{
		char long_title[400];
		memset(long_title, 'z', sizeof(long_title) - 1);
		long_title[sizeof(long_title) - 1] = '\0';
		n = build_listing(body, long_title, "a", "b");
		assert(raop_metadata_parse_dmap(&m, body, n));
		assert(strlen(m.title) == RAOP_METADATA_TEXT_MAX - 1);
	}

	raop_metadata_clear(&m);
	assert(m.title[0] == '\0' && m.has_progress == 0);
	assert(m.text_updates == 4);
}

static void test_progress(void)
{
	raop_metadata m;
	const char *body = "volume: -12.0\r\nprogress: 1146221540/1146549156/1157869601\r\n";
	memset(&m, 0, sizeof(m));
	assert(raop_metadata_parse_progress(&m, body, strlen(body)));
	assert(m.has_progress == 1);
	assert(m.progress_start == 1146221540U);
	assert(m.progress_current == 1146549156U);
	assert(m.progress_end == 1157869601U);
	assert(!raop_metadata_parse_progress(&m, "progress: 1/2\r\n", 15));
	assert(!raop_metadata_parse_progress(&m, "progress: 1/2/99999999999\r\n", 27));
	assert(!raop_metadata_parse_progress(&m, "volume: -3\r\n", 12));
	assert(m.progress_updates == 1);
	assert(raop_metadata_parse_progress(&m, "Progress : 5/6/7", 16));
	assert(m.progress_current == 6U && m.progress_updates == 2);
}

static void session_request(raop_session *session, const char *head,
	const void *body, size_t body_length, const char *expect)
{
	char message[4096];
	char response[RAOP_SESSION_RESPONSE_MAX];
	int count = snprintf(message, sizeof(message), "%sContent-Length: %zu\r\n\r\n",
		head, body_length);
	assert(count > 0 && (size_t)count + body_length < sizeof(message));
	memcpy(message + count, body, body_length);
	assert(raop_session_handle(session, message, (size_t)count + body_length,
		response, sizeof(response)));
	assert(strstr(response, expect) != NULL);
}

static void test_session(void)
{
	raop_session session;
	uint8_t body[2048];
	size_t n;
	const char *sdp = "v=0\r\nm=audio 0 RTP/AVP 96";
	raop_session_init(&session, 6000, 6001, 6002);
	session_request(&session, "ANNOUNCE rtsp://ps3/s RTSP/1.0\r\nCSeq: 2\r\n"
		"Content-Type: application/sdp\r\n", sdp, strlen(sdp), "RTSP/1.0 200");
	session_request(&session, "SETUP rtsp://ps3/s RTSP/1.0\r\nCSeq: 3\r\n"
		"Transport: RTP/AVP/UDP;unicast;mode=record;control_port=5001;"
		"timing_port=5002\r\n", "", 0, "RTSP/1.0 200");
	session_request(&session, "RECORD rtsp://ps3/s RTSP/1.0\r\nCSeq: 4\r\n",
		"", 0, "RTSP/1.0 200");

	n = build_listing(body, "Song \xe2\x80\x94 Live", "Band", "Album");
	session_request(&session, "SET_PARAMETER rtsp://ps3/s RTSP/1.0\r\nCSeq: 5\r\n"
		"Content-Type: application/x-dmap-tagged\r\n", body, n, "RTSP/1.0 200");
	assert(strcmp(session.metadata.title, "Song \xe2\x80\x94 Live") == 0);
	assert(strcmp(session.metadata.artist, "Band") == 0);

	/* Malformed metadata is ignored but still acknowledged. */
	session_request(&session, "SET_PARAMETER rtsp://ps3/s RTSP/1.0\r\nCSeq: 6\r\n"
		"Content-Type: application/x-dmap-tagged\r\n", body, 11, "RTSP/1.0 200");
	assert(strcmp(session.metadata.title, "Song \xe2\x80\x94 Live") == 0);

	/* Artwork is not advertised; if sent anyway it is acknowledged and dropped. */
	session_request(&session, "SET_PARAMETER rtsp://ps3/s RTSP/1.0\r\nCSeq: 7\r\n"
		"Content-Type: image/jpeg\r\n", "\xff\xd8\xff\xe0", 4, "RTSP/1.0 200");

	session_request(&session, "SET_PARAMETER rtsp://ps3/s RTSP/1.0\r\nCSeq: 8\r\n"
		"Content-Type: text/parameters\r\n", "progress: 100/44200/441100\r\n", 28,
		"RTSP/1.0 200");
	assert(session.metadata.has_progress);
	assert(session.metadata.progress_current == 44200U);

	/* Volume still works alongside progress. */
	session_request(&session, "SET_PARAMETER rtsp://ps3/s RTSP/1.0\r\nCSeq: 9\r\n"
		"Content-Type: text/parameters\r\n", "volume: -15.0\r\n", 15, "RTSP/1.0 200");
	assert(session.volume_millidb == -15000);

	session_request(&session, "TEARDOWN rtsp://ps3/s RTSP/1.0\r\nCSeq: 10\r\n",
		"", 0, "RTSP/1.0 200");
	assert(session.metadata.title[0] == '\0');
	assert(!session.metadata.has_progress);
}

int main(void)
{
	test_copy_text();
	test_dmap();
	test_progress();
	test_session();
	printf("raop_metadata_test passed\n");
	return 0;
}

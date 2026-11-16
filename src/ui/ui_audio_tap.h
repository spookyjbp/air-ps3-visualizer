#ifndef PS3_AIRPLAY_UI_AUDIO_TAP_H
#define PS3_AIRPLAY_UI_AUDIO_TAP_H

#include <stddef.h>
#include <stdint.h>

/* Single-producer/single-consumer copy of recently rendered stereo audio, for
 * visualization only. The audio thread pushes; the render thread snapshots.
 * A reader racing the writer may see a partially updated block, which is
 * harmless for drawing and keeps the audio path wait-free. */
#define UI_AUDIO_TAP_FRAMES 8192U /* power of two */

typedef struct {
	float left[UI_AUDIO_TAP_FRAMES];
	float right[UI_AUDIO_TAP_FRAMES];
	volatile uint32_t write_index;
} ui_audio_tap;

void ui_audio_tap_init(ui_audio_tap *tap);
/* Interleaved samples; channels is 1 or 2. */
void ui_audio_tap_push(ui_audio_tap *tap, const float *interleaved,
	size_t frames, unsigned int channels);
/* Copies the newest `frames` frames that end `delay` frames before the write
 * position. Returns the write index observed. */
uint32_t ui_audio_tap_snapshot(const ui_audio_tap *tap, float *left,
	float *right, size_t frames, size_t delay);

#endif

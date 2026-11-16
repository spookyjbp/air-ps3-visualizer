#include "ui_audio_tap.h"

#include <string.h>

#define TAP_MASK (UI_AUDIO_TAP_FRAMES - 1U)

void ui_audio_tap_init(ui_audio_tap *tap)
{
	memset(tap, 0, sizeof(*tap));
}

void ui_audio_tap_push(ui_audio_tap *tap, const float *interleaved,
	size_t frames, unsigned int channels)
{
	uint32_t w = tap->write_index;
	size_t i;
	if (interleaved == NULL || channels == 0)
		return;
	for (i = 0; i < frames; ++i) {
		uint32_t slot = (w + (uint32_t)i) & TAP_MASK;
		float l = interleaved[i * channels];
		tap->left[slot] = l;
		tap->right[slot] = channels > 1 ? interleaved[i * channels + 1] : l;
	}
	__sync_synchronize();
	tap->write_index = w + (uint32_t)frames;
}

uint32_t ui_audio_tap_snapshot(const ui_audio_tap *tap, float *left,
	float *right, size_t frames, size_t delay)
{
	uint32_t w = tap->write_index;
	uint32_t start;
	size_t i;
	__sync_synchronize();
	if (frames + delay > UI_AUDIO_TAP_FRAMES - 512U)
		delay = UI_AUDIO_TAP_FRAMES - 512U > frames ?
			UI_AUDIO_TAP_FRAMES - 512U - frames : 0;
	start = w - (uint32_t)(frames + delay);
	for (i = 0; i < frames; ++i) {
		uint32_t slot = (start + (uint32_t)i) & TAP_MASK;
		left[i] = tap->left[slot];
		right[i] = tap->right[slot];
	}
	return w;
}

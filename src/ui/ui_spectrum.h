#ifndef PS3_AIRPLAY_UI_SPECTRUM_H
#define PS3_AIRPLAY_UI_SPECTRUM_H

#include <stdint.h>

#define UI_FFT_SIZE 1024
#define UI_BANDS 64

typedef struct {
	/* Smoothed band levels in 0..1, low to high frequency (log spaced). */
	float bands[UI_BANDS];
	float peaks[UI_BANDS];
	float peak_velocity[UI_BANDS];
	/* Broad energy in 0..1, smoothed. */
	float level;
	float bass;
	float mids;
	float treble;
	/* 0..1 envelope that jumps to 1 on a detected beat and decays. */
	float beat;
	/* Latest raw input window (for oscilloscope modes). */
	float wave_left[UI_FFT_SIZE];
	float wave_right[UI_FFT_SIZE];

	/* internal */
	float window[UI_FFT_SIZE];
	float re[UI_FFT_SIZE];
	float im[UI_FFT_SIZE];
	float twiddle_re[UI_FFT_SIZE / 2];
	float twiddle_im[UI_FFT_SIZE / 2];
	uint16_t bitrev[UI_FFT_SIZE];
	float band_lo[UI_BANDS];
	float band_hi[UI_BANDS];
	float band_tilt_db[UI_BANDS];
	float bass_average;
	float since_beat;
	float silence_time;
} ui_spectrum;

void ui_spectrum_init(ui_spectrum *s, float sample_rate);
/* Analyzes UI_FFT_SIZE stereo frames; dt is seconds since the last update. */
void ui_spectrum_update(ui_spectrum *s, const float *left, const float *right,
	float dt);

#endif

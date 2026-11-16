#include "ui_spectrum.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979f
#define BAND_MIN_HZ 35.0f
#define BAND_MAX_HZ 16000.0f
#define FLOOR_DB -62.0f
#define CEIL_DB -6.0f

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

void ui_spectrum_init(ui_spectrum *s, float sample_rate)
{
	int i, bits = 0;
	float bin_hz = sample_rate / (float)UI_FFT_SIZE;
	memset(s, 0, sizeof(*s));
	while ((1 << bits) < UI_FFT_SIZE)
		++bits;
	for (i = 0; i < UI_FFT_SIZE; ++i) {
		int r = 0, b;
		for (b = 0; b < bits; ++b)
			if (i & (1 << b))
				r |= 1 << (bits - 1 - b);
		s->bitrev[i] = (uint16_t)r;
		s->window[i] = 0.5f - 0.5f * cosf(2.0f * PI_F * (float)i /
			(float)(UI_FFT_SIZE - 1));
	}
	for (i = 0; i < UI_FFT_SIZE / 2; ++i) {
		s->twiddle_re[i] = cosf(-2.0f * PI_F * (float)i / (float)UI_FFT_SIZE);
		s->twiddle_im[i] = sinf(-2.0f * PI_F * (float)i / (float)UI_FFT_SIZE);
	}
	for (i = 0; i < UI_BANDS; ++i) {
		float f0 = BAND_MIN_HZ * powf(BAND_MAX_HZ / BAND_MIN_HZ,
			(float)i / (float)UI_BANDS);
		float f1 = BAND_MIN_HZ * powf(BAND_MAX_HZ / BAND_MIN_HZ,
			(float)(i + 1) / (float)UI_BANDS);
		float center = sqrtf(f0 * f1);
		s->band_lo[i] = f0 / bin_hz;
		s->band_hi[i] = f1 / bin_hz;
		/* Music falls off ~3-4.5 dB/octave; tilt so the display looks even. */
		s->band_tilt_db[i] = 4.0f * log2f(center / 1000.0f);
	}
	s->bass_average = 0.05f;
}

static void fft(ui_spectrum *s)
{
	int size, i, j;
	for (i = 0; i < UI_FFT_SIZE; ++i) {
		int r = s->bitrev[i];
		if (r > i) {
			float t = s->re[i]; s->re[i] = s->re[r]; s->re[r] = t;
			t = s->im[i]; s->im[i] = s->im[r]; s->im[r] = t;
		}
	}
	for (size = 2; size <= UI_FFT_SIZE; size <<= 1) {
		int half = size >> 1, step = UI_FFT_SIZE / size;
		for (i = 0; i < UI_FFT_SIZE; i += size) {
			for (j = 0; j < half; ++j) {
				float wr = s->twiddle_re[j * step], wi = s->twiddle_im[j * step];
				int a = i + j, b = a + half;
				float tr = s->re[b] * wr - s->im[b] * wi;
				float ti = s->re[b] * wi + s->im[b] * wr;
				s->re[b] = s->re[a] - tr;
				s->im[b] = s->im[a] - ti;
				s->re[a] += tr;
				s->im[a] += ti;
			}
		}
	}
}

/* Magnitude at a fractional bin, linearly interpolated (for narrow bass bands). */
static float magnitude_at(const ui_spectrum *s, float bin)
{
	int i = (int)bin;
	float f = bin - (float)i;
	float a, b;
	if (i < 1) { i = 1; f = 0.0f; }
	if (i >= UI_FFT_SIZE / 2 - 1) { i = UI_FFT_SIZE / 2 - 2; f = 1.0f; }
	a = sqrtf(s->re[i] * s->re[i] + s->im[i] * s->im[i]);
	b = sqrtf(s->re[i + 1] * s->re[i + 1] + s->im[i + 1] * s->im[i + 1]);
	return a + (b - a) * f;
}

static float band_magnitude(const ui_spectrum *s, int band)
{
	float lo = s->band_lo[band], hi = s->band_hi[band];
	float peak = 0.0f;
	int i;
	if (hi - lo < 1.0f)
		return magnitude_at(s, (lo + hi) * 0.5f);
	for (i = (int)ceilf(lo); i <= (int)hi && i < UI_FFT_SIZE / 2; ++i) {
		float m = sqrtf(s->re[i] * s->re[i] + s->im[i] * s->im[i]);
		if (m > peak)
			peak = m;
	}
	return peak;
}

void ui_spectrum_update(ui_spectrum *s, const float *left, const float *right,
	float dt)
{
	int i;
	float scale = 4.0f / (float)UI_FFT_SIZE; /* full-scale Hann sine -> 1.0 */
	float sum_sq = 0.0f, bass = 0.0f, mids = 0.0f, treble = 0.0f;
	float attack, decay, energy_decay;
	if (dt <= 0.0f) dt = 1.0f / 60.0f;
	if (dt > 0.25f) dt = 0.25f;
	attack = 1.0f - expf(-dt / 0.025f);
	decay = 1.0f - expf(-dt / 0.16f);
	energy_decay = 1.0f - expf(-dt / 0.12f);

	for (i = 0; i < UI_FFT_SIZE; ++i) {
		float l = left[i], r = right[i];
		float m = 0.5f * (l + r);
		s->wave_left[i] = l;
		s->wave_right[i] = r;
		sum_sq += m * m;
		s->re[i] = m * s->window[i];
		s->im[i] = 0.0f;
	}
	fft(s);

	for (i = 0; i < UI_BANDS; ++i) {
		float mag = band_magnitude(s, i) * scale;
		float db = 20.0f * log10f(mag + 1e-7f) + s->band_tilt_db[i];
		float target = clampf((db - FLOOR_DB) / (CEIL_DB - FLOOR_DB), 0.0f, 1.0f);
		float k = target > s->bands[i] ? attack : decay;
		target = target * target * (3.0f - 2.0f * target) * 0.35f + target * 0.65f;
		s->bands[i] += (target - s->bands[i]) * k;
		if (s->bands[i] >= s->peaks[i]) {
			s->peaks[i] = s->bands[i];
			s->peak_velocity[i] = 0.0f;
		} else {
			s->peak_velocity[i] += 1.6f * dt;
			s->peaks[i] -= s->peak_velocity[i] * dt;
			if (s->peaks[i] < s->bands[i])
				s->peaks[i] = s->bands[i];
		}
		if (i < UI_BANDS * 18 / 100)
			bass += s->bands[i];
		else if (i < UI_BANDS * 65 / 100)
			mids += s->bands[i];
		else
			treble += s->bands[i];
	}
	bass /= (float)(UI_BANDS * 18 / 100);
	mids /= (float)(UI_BANDS * 65 / 100 - UI_BANDS * 18 / 100);
	treble /= (float)(UI_BANDS - UI_BANDS * 65 / 100);

	s->level += (clampf(sqrtf(sum_sq / (float)UI_FFT_SIZE) * 3.0f, 0.0f, 1.0f)
		- s->level) * energy_decay;
	s->mids += (mids - s->mids) * energy_decay;
	s->treble += (treble - s->treble) * energy_decay;

	/* Beat: bass rising well above its recent average. */
	s->since_beat += dt;
	if (bass > s->bass_average * 1.28f + 0.04f && bass > 0.22f &&
		s->since_beat > 0.22f) {
		s->beat = 1.0f;
		s->since_beat = 0.0f;
	} else {
		s->beat *= expf(-dt / 0.18f);
	}
	s->bass_average += (bass - s->bass_average) * (1.0f - expf(-dt / 0.6f));
	s->bass += (bass - s->bass) * (bass > s->bass ? attack : energy_decay);

	if (s->level < 0.002f)
		s->silence_time += dt;
	else
		s->silence_time = 0.0f;
}

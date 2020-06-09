#include "effect_biquad.h"
#include "AudioStream.h"
#include <math.h>
#include "Arduino.h"

void IRAM_ATTR AudioFilterBiquad::update(void)
{
	audio_block_t *block;

	block = receiveWritable(0);

	if (!block) {
		return;
	}

  // biquad filtering is based on a small sliding window, where the different filters are a result of
  // simply changing the coefficients used while processing the samples
  //
  // the biquad filter processes a sound using 10 parameters:
  //   b0, b1, b2, a1, a2      transformation coefficients
  //   xn0, xn1, xn2           the unfiltered sample at position x[n], x[n-1], and x[n-2]
  //   yn1, yn2                the filtered sample at position y[n-1] and y[n-2]
  //void sf_biquad_process(sf_biquad_state_st *state, int size, audio_block_t *input,
  //	audio_block_t *output){

	// pull out the state into local variables
	float b0  = biquadState.b0;
	float b1  = biquadState.b1;
	float b2  = biquadState.b2;
	float a1  = biquadState.a1;
	float a2  = biquadState.a2;
	float xn1 = biquadState.xn1;
	float xn2 = biquadState.xn2;
	float yn1 = biquadState.yn1;
	float yn2 = biquadState.yn2;

	// loop for each sample
	for (int n = 0; n < AUDIO_BLOCK_SAMPLES; n++){
		// get the current sample
		float xn0 = block->data[n];

		// the formula is the same for each channel
		block->data[n] = 
			b0 * xn0 +
			b1 * xn1 +
			b2 * xn2 -
			a1 * yn1 -
			a2 * yn2;

		// slide everything down one sample
		xn2 = xn1;
		xn1 = xn0;
		yn2 = yn1;
		yn1 = block->data[n];
	}

	// save the state for future processing
	biquadState.xn1 = xn1;
	biquadState.xn2 = xn2;
	biquadState.yn1 = yn1;
	biquadState.yn2 = yn2;

	transmit(block);
	release(block);
}

// clear the samples saved across process boundaries
void AudioFilterBiquad::state_reset(){
	biquadState.xn1 = 0;
	biquadState.xn2 = 0;
	biquadState.yn1 = 0;
	biquadState.yn2 = 0;
}

// set the coefficients so that the output is the input scaled by `amt`
void AudioFilterBiquad::state_scale(float amt){
	biquadState.b0 = amt;
	biquadState.b1 = 0.0f;
	biquadState.b2 = 0.0f;
	biquadState.a1 = 0.0f;
	biquadState.a2 = 0.0f;
}

// set the coefficients so that the output is an exact copy of the input
void AudioFilterBiquad::state_passthrough(){
	state_scale(1.0f);
}

// set the coefficients so that the output is zeroed out
void AudioFilterBiquad::state_zero(){
	state_scale(0.0f);
}

// initialize the biquad state to be a lowpass filter
void AudioFilterBiquad::lowpass(float cutoff, float resonance){
	state_reset();
	float nyquist = AUDIO_SAMPLE_RATE_EXACT * 0.5f;
	cutoff /= nyquist;

	if (cutoff >= 1.0f)
		state_passthrough();
	else if (cutoff <= 0.0f)
		state_zero();
	else{
		resonance = powf(10.0f, resonance * 0.05f); // convert resonance from dB to linear
		float theta = (float)PI * 2.0f * cutoff;
		float alpha = sinf(theta) / (2.0f * resonance);
		float cosw  = cosf(theta);
		float beta  = (1.0f - cosw) * 0.5f;
		float a0inv = 1.0f / (1.0f + alpha);
		biquadState.b0 = a0inv * beta;
		biquadState.b1 = a0inv * 2.0f * beta;
		biquadState.b2 = a0inv * beta;
		biquadState.a1 = a0inv * -2.0f * cosw;
		biquadState.a2 = a0inv * (1.0f - alpha);
	}
}

void AudioFilterBiquad::highpass(float cutoff, float resonance){
	state_reset();
	float nyquist = AUDIO_SAMPLE_RATE_EXACT * 0.5f;
	cutoff /= nyquist;

	if (cutoff >= 1.0f)
		state_zero();
	else if (cutoff <= 0.0f)
		state_passthrough();
	else{
		resonance = powf(10.0f, resonance * 0.05f); // convert resonance from dB to linear
		float theta = (float)PI * 2.0f * cutoff;
		float alpha = sinf(theta) / (2.0f * resonance);
		float cosw  = cosf(theta);
		float beta  = (1.0f + cosw) * 0.5f;
		float a0inv = 1.0f / (1.0f + alpha);
		biquadState.b0 = a0inv * beta;
		biquadState.b1 = a0inv * -2.0f * beta;
		biquadState.b2 = a0inv * beta;
		biquadState.a1 = a0inv * -2.0f * cosw;
		biquadState.a2 = a0inv * (1.0f - alpha);
	}
}

void AudioFilterBiquad::bandpass(float freq, float Q){
	state_reset();
	float nyquist = AUDIO_SAMPLE_RATE_EXACT * 0.5f;
	freq /= nyquist;

	if (freq <= 0.0f || freq >= 1.0f)
		state_zero();
	else if (Q <= 0.0f)
		state_passthrough();
	else{
		float w0    = (float)PI * 2.0f * freq;
		float alpha = sinf(w0) / (2.0f * Q);
		float k     = cosf(w0);
		float a0inv = 1.0f / (1.0f + alpha);
		biquadState.b0 = a0inv * alpha;
		biquadState.b1 = 0;
		biquadState.b2 = a0inv * -alpha;
		biquadState.a1 = a0inv * -2.0f * k;
		biquadState.a2 = a0inv * (1.0f - alpha);
	}
}

void AudioFilterBiquad::notch(float freq, float Q){
	state_reset();
	float nyquist = AUDIO_SAMPLE_RATE_EXACT * 0.5f;
	freq /= nyquist;

	if (freq <= 0.0f || freq >= 1.0f)
		state_passthrough();
	else if (Q <= 0.0f)
		state_zero();
	else{
		float w0    = (float)PI * 2.0f * freq;
		float alpha = sinf(w0) / (2.0f * Q);
		float k     = cosf(w0);
		float a0inv = 1.0f / (1.0f + alpha);
		biquadState.b0 = a0inv;
		biquadState.b1 = a0inv * -2.0f * k;
		biquadState.b2 = a0inv;
		biquadState.a1 = a0inv * -2.0f * k;
		biquadState.a2 = a0inv * (1.0f - alpha);
	}
}

void AudioFilterBiquad::peaking(float freq, float Q, float gain){
	state_reset();
	float nyquist = AUDIO_SAMPLE_RATE_EXACT * 0.5f;
	freq /= nyquist;

	if (freq <= 0.0f || freq >= 1.0f){
		state_passthrough();
		return;
	}

	float A = powf(10.0f, gain * 0.025f); // square root of gain converted from dB to linear

	if (Q <= 0.0f){
		state_scale(A * A); // scale by A squared
		return;
	}

	float w0    = (float)PI * 2.0f * freq;
	float alpha = sinf(w0) / (2.0f * Q);
	float k     = cosf(w0);
	float a0inv = 1.0f / (1.0f + alpha / A);
	biquadState.b0 = a0inv * (1.0f + alpha * A);
	biquadState.b1 = a0inv * -2.0f * k;
	biquadState.b2 = a0inv * (1.0f - alpha * A);
	biquadState.a1 = a0inv * -2.0f * k;
	biquadState.a2 = a0inv * (1.0f - alpha / A);
}

void AudioFilterBiquad::allpass(float freq, float Q){
	state_reset();
	float nyquist = AUDIO_SAMPLE_RATE_EXACT * 0.5f;
	freq /= nyquist;

	if (freq <= 0.0f || freq >= 1.0f)
		state_passthrough();
	else if (Q <= 0.0f)
		state_scale(-1.0f); // invert the sample
	else{
		float w0    = (float)PI * 2.0f * freq;
		float alpha = sinf(w0) / (2.0f * Q);
		float k     = cosf(w0);
		float a0inv = 1.0f / (1.0f + alpha);
		biquadState.b0 = a0inv * (1.0f - alpha);
		biquadState.b1 = a0inv * -2.0f * k;
		biquadState.b2 = a0inv * (1.0f + alpha);
		biquadState.a1 = a0inv * -2.0f * k;
		biquadState.a2 = a0inv * (1.0f - alpha);
	}
}

void AudioFilterBiquad::lowshelf(float freq, float Q, float gain){
	state_reset();
	float nyquist = AUDIO_SAMPLE_RATE_EXACT * 0.5f;
	freq /= nyquist;

	if (freq <= 0.0f || Q == 0.0f){
		state_passthrough();
		return;
	}

	float A = powf(10.0f, gain * 0.025f); // square root of gain converted from dB to linear

	if (freq >= 1.0f){
		state_scale(A * A); // scale by A squared
		return;
	}

	float w0    = (float)PI * 2.0f * freq;
	float ainn  = (A + 1.0f / A) * (1.0f / Q - 1.0f) + 2.0f;
	if (ainn < 0)
		ainn = 0;
	float alpha = 0.5f * sinf(w0) * sqrtf(ainn);
	float k     = cosf(w0);
	float k2    = 2.0f * sqrtf(A) * alpha;
	float Ap1   = A + 1.0f;
	float Am1   = A - 1.0f;
	float a0inv = 1.0f / (Ap1 + Am1 * k + k2);
	biquadState.b0 = a0inv * A * (Ap1 - Am1 * k + k2);
	biquadState.b1 = a0inv * 2.0f * A * (Am1 - Ap1 * k);
	biquadState.b2 = a0inv * A * (Ap1 - Am1 * k - k2);
	biquadState.a1 = a0inv * -2.0f * (Am1 + Ap1 * k);
	biquadState.a2 = a0inv * (Ap1 + Am1 * k - k2);
}

void AudioFilterBiquad::highshelf(float freq, float Q, float gain){
	state_reset();
	float nyquist = AUDIO_SAMPLE_RATE_EXACT * 0.5f;
	freq /= nyquist;

	if (freq >= 1.0f || Q == 0.0f){
		state_passthrough();
		return;
	}

	float A = powf(10.0f, gain * 0.025f); // square root of gain converted from dB to linear

	if (freq <= 0.0f){
		state_scale(A * A); // scale by A squared
		return;
	}

	float w0    = (float)PI * 2.0f * freq;
	float ainn  = (A + 1.0f / A) * (1.0f / Q - 1.0f) + 2.0f;
	if (ainn < 0)
		ainn = 0;
	float alpha = 0.5f * sinf(w0) * sqrtf(ainn);
	float k     = cosf(w0);
	float k2    = 2.0f * sqrtf(A) * alpha;
	float Ap1   = A + 1.0f;
	float Am1   = A - 1.0f;
	float a0inv = 1.0f / (Ap1 - Am1 * k + k2);
	biquadState.b0 = a0inv * A * (Ap1 + Am1 * k + k2);
	biquadState.b1 = a0inv * -2.0f * A * (Am1 + Ap1 * k);
	biquadState.b2 = a0inv * A * (Ap1 + Am1 * k - k2);
	biquadState.a1 = a0inv * 2.0f * (Am1 - Ap1 * k);
	biquadState.a2 = a0inv * (Ap1 - Am1 * k - k2);
}
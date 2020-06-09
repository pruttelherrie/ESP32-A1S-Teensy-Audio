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
//}


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


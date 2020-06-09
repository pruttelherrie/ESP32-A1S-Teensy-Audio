#ifndef effect_biquad_h_
#define effect_biquad_h_

#include "AudioStream.h"
#include "Arduino.h"
//#include "../lib/sndfilter/biquad.h"

typedef struct {
	float b0;
	float b1;
	float b2;
	float a1;
	float a2;
	float xn1;
	float xn2;
	float yn1;
	float yn2;
} biquad_state_st;

class AudioFilterBiquad : public AudioStream
{
public:
	AudioFilterBiquad(void) : AudioStream(1, inputQueueArray, "AudioEffectBiquad") { }

    void lowpass( float cutoff, float resonance);
    void highpass( float cutoff, float resonance);
    void bandpass ( float freq, float Q);
    void notch    ( float freq, float Q);
    void peaking  ( float freq, float Q, float gain);
    void allpass  ( float freq, float Q);
    void lowshelf ( float freq, float Q, float gain);
    void highshelf( float freq, float Q, float gain);
    
	virtual void update(void);

private:
    void state_reset();
    void state_scale(float amt);
    void state_passthrough();
    void state_zero();
	audio_block_t *inputQueueArray[1];
    biquad_state_st biquadState;
};

#endif
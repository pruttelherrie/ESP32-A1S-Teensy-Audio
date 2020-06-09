/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef effect_delay_ext_h_
#define effect_delay_ext_h_
#include "Arduino.h"
#include "AudioStream.h"

enum AudioEffectDelayMemoryType_t {
	AUDIO_MEMORY_SPIRAM = 0,	
	AUDIO_MEMORY_UNDEFINED = 1
};

class AudioEffectDelayExternal : public AudioStream
{
public:
	AudioEffectDelayExternal() : AudioStream(1, inputQueueArray, "AudioEffectDelayExternal") { } 
  boolean delay(uint8_t channel, float milliseconds) {
		if (channel >= 8 || memory_type >= AUDIO_MEMORY_UNDEFINED) return true;
		if (milliseconds < 0.0) milliseconds = 0.0;
		uint32_t n = (milliseconds*(AUDIO_SAMPLE_RATE_EXACT/1000.0f))+0.5f;
		n += AUDIO_BLOCK_SAMPLES;
		if (n > memory_length - AUDIO_BLOCK_SAMPLES)
			n = memory_length - AUDIO_BLOCK_SAMPLES;
		delay_length[channel] = n;
		uint8_t mask = activemask;
		activemask = mask | (1<<channel);
		return false;
	}
	void disable(uint8_t channel) {
		if (channel >= 8) return;
		uint8_t mask = activemask & ~(1<<channel);
		activemask = mask;
	}
	virtual void update(void);
	void initialize(uint32_t samples);
private:

	void read(uint32_t address, uint32_t count, float *data);
	void write(uint32_t address, uint32_t count, const float *data);
	void zero(uint32_t address, uint32_t count) {
		uint32_t addr = memory_begin + address;
    memset((void *)addr, 0, count*sizeof(float));
	}
	uint32_t memory_begin;    // the first address in the memory we're using
	uint32_t memory_length;   // the amount of memory we're using
	uint32_t head_offset;     // head index (incoming) data into external memory
	uint32_t delay_length[8]; // # of sample delay for each channel (128 = no delay)
	uint8_t  activemask;      // which output channels are active
	uint8_t  memory_type;     // 0=SPIRAM
	audio_block_t *inputQueueArray[1];
};

#endif

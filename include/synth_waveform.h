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

#ifndef synth_waveform_h_
#define synth_waveform_h_

#include <Arduino.h>
#include "AudioStream.h"

// src/data_waveforms.c
extern "C" {
extern const float AudioWaveformSine[257];
}

#define WAVEFORM_SINE              0
#define WAVEFORM_SAWTOOTH          1
#define WAVEFORM_SQUARE            2
#define WAVEFORM_TRIANGLE          3
#define WAVEFORM_ARBITRARY         4
#define WAVEFORM_PULSE             5
#define WAVEFORM_SAWTOOTH_REVERSE  6
#define WAVEFORM_SAMPLE_HOLD       7
#define WAVEFORM_TRIANGLE_VARIABLE 8

class AudioSynthWaveform : public AudioStream {
public:
	AudioSynthWaveform(void) : AudioStream(0,NULL, "AudioSynthWaveform"),
		phase_accumulator(0), phase_increment(0), phase_offset(0),
		magnitude(0), pulse_width(0.5),
		arbdata(NULL), sample(0), tone_type(WAVEFORM_SINE),
		tone_offset(0) {
	}

	void frequency(float freq) {
		if (freq < 0.0) {
			freq = 0.0;
		} else if (freq > AUDIO_SAMPLE_RATE_EXACT / 2) {
			freq = AUDIO_SAMPLE_RATE_EXACT / 2;
		}
		phase_increment = freq * (1.0 / AUDIO_SAMPLE_RATE_EXACT);
		//if (phase_increment > 0x7FFE0000u) phase_increment = 0x7FFE0000;
		if (phase_increment > 0.5) phase_increment = 0.5; // FIXME
	}
	void phase(float angle) {
		if (angle < 0.0) {
			angle = 0.0;
		} else if (angle >= 360.0) {
			angle = angle - 360.0;
			if (angle >= 360.0) return;
		}
		//phase_offset = angle * (4294967296.0 / 360.0);
		phase_offset = angle * (1.0 / 360.0);
	}
	void amplitude(float n) {	// 0 to 1.0
		if (n < 0) {
			n = 0;
		} else if (n > 1.0) {
			n = 1.0;
		}
		//magnitude = n * 65536.0;
    magnitude = n;
	}
	void offset(float n) {
		if (n < -1.0) {
			n = -1.0;
		} else if (n > 1.0) {
			n = 1.0;
		}
		//tone_offset = n * 32767.0;
		tone_offset = n;
	}
	void pulseWidth(float n) {	// 0.0 to 1.0
		if (n <= 0) {
			n = 0.000001;
		} else if (n >= 1.0) {
			n = 0.999999;
		}
		//pulse_width = n * 4294967296.0;
		pulse_width = n;
	}
	void begin(short t_type) {
		phase_offset = 0;
		tone_type = t_type;
	}
	void begin(float t_amp, float t_freq, short t_type) {
		amplitude(t_amp);
		frequency(t_freq);
		phase_offset = 0;
		tone_type = t_type;
	}
	void arbitraryWaveform(const float *data, float maxFreq) {
		arbdata = data;
	}
	virtual void update(void);

private:
	float phase_accumulator;
	float phase_increment;
	float phase_offset;
	float magnitude;
	float pulse_width;
	const float *arbdata;
	float sample; // for WAVEFORM_SAMPLE_HOLD
	short tone_type;
	float tone_offset;
};


class AudioSynthWaveformModulated : public AudioStream {
public:
	AudioSynthWaveformModulated(void) : AudioStream(2, inputQueueArray,"AudioSynthWaveformModulated"), 
		phase_accumulator(0), phase_increment(0), modulation_factor(32768),
		magnitude(0), arbdata(NULL), sample(0), tone_offset(0),
		tone_type(WAVEFORM_SINE), modulation_type(0) {
	}

	void frequency(float freq) {
		if (freq < 0.0) {
			freq = 0.0;
		} else if (freq > AUDIO_SAMPLE_RATE_EXACT / 2) {
			freq = AUDIO_SAMPLE_RATE_EXACT / 2;
		}
		phase_increment = freq * (4294967296.0 / AUDIO_SAMPLE_RATE_EXACT);
		if (phase_increment > 0x7FFE0000u) phase_increment = 0x7FFE0000;
	}
	void amplitude(float n) {	// 0 to 1.0
		if (n < 0) {
			n = 0;
		} else if (n > 1.0) {
			n = 1.0;
		}
		magnitude = n * 65536.0;
	}
	void offset(float n) {
		if (n < -1.0) {
			n = -1.0;
		} else if (n > 1.0) {
			n = 1.0;
		}
		tone_offset = n * 32767.0;
	}
	void begin(short t_type) {
		tone_type = t_type;
	}
	void begin(float t_amp, float t_freq, short t_type) {
		amplitude(t_amp);
		frequency(t_freq);
		tone_type = t_type;
	}
	void arbitraryWaveform(const float *data, float maxFreq) {
		arbdata = data;
	}
	void frequencyModulation(float octaves) {
		if (octaves > 12.0) {
			octaves = 12.0;
		} else if (octaves < 0.1) {
			octaves = 0.1;
		}
		modulation_factor = octaves * 4096.0;
		modulation_type = 0;
	}
	void phaseModulation(float degrees) {
		if (degrees > 9000.0) {
			degrees = 9000.0;
		} else if (degrees < 30.0) {
			degrees = 30.0;
		}
		modulation_factor = degrees * (65536.0 / 180.0);
		modulation_type = 1;
	}
	virtual void update(void);

private:
	audio_block_t *inputQueueArray[2];
	float phase_accumulator;
	float phase_increment;
	float modulation_factor;
	float  magnitude;
	const float *arbdata;
	float phasedata[AUDIO_BLOCK_SAMPLES];
	float  sample; // for WAVEFORM_SAMPLE_HOLD
	int16_t  tone_offset;
	uint8_t  tone_type;
	uint8_t  modulation_type;
};


#endif

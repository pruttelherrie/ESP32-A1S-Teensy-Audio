#ifndef synth_dc_h_
#define synth_dc_h_

#include "AudioStream.h"
#include "freertos/FreeRTOS.h"

class AudioSynthWaveformDc : public AudioStream
{
//GUI: inputs:0, outputs:1 //this line used for automatic generation of GUI node
//GUI: shortName:dc  //this line used for automatic generation of GUI node
public:
	AudioSynthWaveformDc() : AudioStream(0, NULL, "AudioSynthWaveformDC"), dcValue(0.0) { initialised = true; }
	virtual void update(void);
    void value(float n) {
		if (n < -1.0f) n = -1.0f;
		else if (n > 1.0f) n = 1.0f;
		dcValue = n;
	}
private:
    float dcValue;
};

#endif
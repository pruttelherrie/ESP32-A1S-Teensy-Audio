// Example: pass-through

#include "Arduino.h"
#include "Audio.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "esp_task_wdt.h"
#include "control_ac101.h"
#include "control_i2s.h"
#include "effect_delay_ext.h"
#include "effect_biquad.h"

#define CODEC_SCK       (GPIO_NUM_32)
#define CODEC_SDA       (GPIO_NUM_33)

//audio processing frame length in samples (L+R) 64 samples (32R+32L) 256 Bytes
#define FRAMELENGTH    64
//sample count per channel for each frame (32)
#define SAMPLECOUNT   FRAMELENGTH/2
//channel count inside a frame (always stereo = 2)
#define CHANNELCOUNT  2
//frame size in bytes
#define FRAMESIZE   FRAMELENGTH*4

AudioControlI2S          i2s;
AudioControlAC101        ac101;


// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=98,278
AudioMixer4              mixer2;         //xy=354,358
AudioFilterBiquad        biquad1;        //xy=514,468
AudioEffectDelayExternal         delay1;         //xy=660,364
AudioAmplifier           amp2;           //xy=727,58
AudioAmplifier           amp1;           //xy=816,328
AudioMixer4              mixer1;         //xy=984,273
AudioOutputI2S           i2s2;           //xy=1220,258
AudioConnection          patchCord1(i2s1, 1, mixer1, 0);
AudioConnection          patchCord2(i2s1, 1, mixer2, 1);
AudioConnection          patchCord3(mixer2, biquad1);
AudioConnection          patchCord4(biquad1, delay1);
AudioConnection          patchCord5(delay1, 0, amp1, 0);
AudioConnection          patchCord6(amp2, 0, mixer2, 0);
AudioConnection          patchCord7(amp1, 0, mixer1, 1);
AudioConnection          patchCord8(mixer1, 0, i2s2, 1);
AudioConnection          patchCord9(mixer1, amp2);
// GUItool: end automatically generated code

// NOTE: Connections are most efficient when made from an earlier 
//       object (in the order they are created) to a later one. 
//       Connections from a later object back to an earlier object 
//       can be made, but they add a 1-block delay and consume more 
//       memory to implement that delay. 

void audioTask( void * parameter ) {
  AudioStream *p;  
  for(;;) {
    p->update_all();    
  }
}

void setup() {
    Serial.begin(115200);
    AudioMemory(40); 

    amp1.gain(0.7);
    amp2.gain(0.4);
    delay1.initialize(100990); 
    delay1.delay(0,2290);
    biquad1.lowpass( 1000.0, 2.0);

    ESP_LOGI(TAG, "I2C");
    if(ac101.begin(CODEC_SDA, CODEC_SCK)) 
      ESP_LOGI(TAG,"AC101.begin success");
    else
      ESP_LOGE(TAG,"AC101.begin FAIL!");
    //stereo unbalanced line-in
    ac101.LeftLineLeft(true);
    ac101.RightLineRight(true);
    ac101.SetVolumeHeadphone(63); // not connected on BlackStomp
    ac101.SetVolumeSpeaker(63);
    vTaskDelay(1000/portTICK_RATE_MS);
    ESP_LOGI(TAG, "I2S");
    i2s.ac101();
    ESP_LOGI(TAG, "Starting audiotask");
    xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 24, NULL, 1);

    pinMode(2,OUTPUT);
    pinMode(5,INPUT_PULLUP);

    ESP_LOGI(TAG, "Setup complete");
}

bool led=false;

void loop () {
  vTaskDelay(500);
  digitalWrite(2,led);
  led=!led;
  if(digitalRead(5)==LOW) {
    Serial.println("Footswitch pressed!");
  }
}

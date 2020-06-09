/* Example: delay in SPIRAM, with lowpass, feedback and OSC control of params
 *
 * OSC paths for TouchOSC (hexler.net), layout "Simple":
 * /1/fader1 = delay time (0-2290ms)
 * /1/fader2 = delay mix (0-100%)
 * /1/fader3 = delay feedback (0-100%)
 * /1/fader4 = delayline lowpass frequency
 * 
 * Copy this file to /src/main.cpp , compile & upload
 * Point TouchOSC at IP, port 9004.
 * Play!
 *
 */ 

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


// ============ START OSC parameter control
#include <ArduinoOSC.h>
const char* ssid = "ssid";
const char* pwd = "password";
const IPAddress ip(192, 168, 1, 100);
const IPAddress gateway(192, 168, 1, 1);
const IPAddress subnet(255, 255, 255, 0);
const int recv_port = 9004;
float f1,f2,f3,f4;
float o_delay,o_mix,o_feedback,o_lp;
// ============ END OSC parameter control


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
AudioConnection          patchCord1(i2s1, 1, mixer1,0);
AudioConnection          patchCord2(i2s1, 1, mixer2, 1);
AudioConnection          patchCord3(mixer2, biquad1);
AudioConnection          patchCord7(biquad1,0, delay1, 0);
AudioConnection          patchCord5(delay1, 0, amp1, 0);
AudioConnection          patchCord6(amp2, 0, mixer2, 0);
AudioConnection          patchCord9(amp1, 0, mixer1, 1);
AudioConnection          patchCord8(mixer1, 0, i2s2, 1);
AudioConnection          patchCord10(mixer1, amp2);
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

    amp1.gain(0.2);
    amp2.gain(0.3);
    delay1.initialize(100990); 
    delay1.delay(0,666);    
    biquad1.lowpass(1800.0, 2.0);

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

    // WiFi stuff (for OSC)
    WiFi.begin(ssid, pwd);
    WiFi.config(ip, gateway, subnet);
    while (WiFi.status() != WL_CONNECTED) { 
        Serial.print("."); 
        delay(500); 
    }
    Serial.print("WiFi connected, IP = "); 
    Serial.println(WiFi.localIP());

    // subscribe osc messages
    OscWiFi.subscribe(recv_port, "/1/fader1", f1);
    OscWiFi.subscribe(recv_port, "/1/fader2", f2);
    OscWiFi.subscribe(recv_port, "/1/fader3", f3);
    OscWiFi.subscribe(recv_port, "/1/fader4", f4);

    // Ready!
    ESP_LOGI(TAG, "Setup complete");
}

bool led=false;

void loop () {
  vTaskDelay(100);

  OscWiFi.update(); // should be called to receive + send osc

  if(f1 != o_delay) {
      o_delay = f1;
      delay1.delay(0, f1 * 2290.0); // TC :)
      Serial.print("Delay time [ms] = ");
      Serial.println(o_delay * 2290.0);
  }
  if(f2 != o_mix) {
      o_mix = f2;
      amp1.gain(f2);
      Serial.print("Delay mix [%] = ");
      Serial.println(f2 * 100.0);
  }
  if(f3 != o_feedback) {
    o_feedback = f3;
    amp2.gain(o_feedback);
    Serial.print("Delay feedback [%] = ");
    Serial.println(o_feedback * 100.0);
  }
  if(f4 != o_lp) {
    o_lp = f4;
    biquad1.lowpass(o_lp * 8000.0, 2.0); // max 8kHz, resonance = 2
    Serial.print("Delay lowpass frequency [Hz] = ");
    Serial.println(o_lp * 8000.0);
  }

  digitalWrite(2,led);
  led=!led;
  if(digitalRead(5)==LOW) {
    Serial.println("Footswitch pressed!");
  }
}

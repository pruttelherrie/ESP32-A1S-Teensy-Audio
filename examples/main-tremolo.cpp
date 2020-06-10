/* Example: tremolo OSC control of amplitude and frequency
 *
 * OSC paths for TouchOSC (hexler.net), layout "Simple":
 * /1/fader1 = amplitude (0-100%)
 * /1/fader2 = frequency (0-10Hz)
 * 
 * Copy this file to /src/ , compile & upload
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
#include "effect_multiply.h"
#include "synth_sine.h"
#include "synth_waveform.h"


// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=263,412
AudioSynthWaveform       waveform1;      //xy=355,518
AudioEffectMultiply      multiply1;      //xy=599,419
AudioOutputI2S           i2s2;           //xy=879,404
AudioConnection          patchCord1(i2s1, 1, multiply1, 0);
AudioConnection          patchCord2(waveform1, 0, multiply1, 1);
AudioConnection          patchCord3(multiply1, 0, i2s2, 1);
// GUItool: end automatically generated code


// NOTE: Connections are most efficient when made from an earlier 
//       object (in the order they are created) to a later one. 
//       Connections from a later object back to an earlier object 
//       can be made, but they add a 1-block delay and consume more 
//       memory to implement that delay. 


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


// ============ START OSC parameter control ============================
#include <ArduinoOSC.h>

// WiFi stuff
const char* ssid = "SSID";
const char* pwd = "passwd";
const IPAddress ip(192, 168, 1, 201);
const IPAddress gateway(192, 168, 1, 1);
const IPAddress subnet(255, 255, 255, 0);

// for ArduinoOSC
//const char* host = "192.168.178.20";
const int recv_port = 9004;
//const int bind_port = 9004;
//const int send_port = 55555;
//const int publish_port = 54445;
// send / receive variables
float f1,f2,f3,f4;
float o_delay,o_mix,o_feedback,o_lp,o_amplitude,o_frequency;
// ============ END OSC parameter control ==========================

AudioControlI2S          i2s;
AudioControlAC101        ac101;

void audioTask( void * parameter ) {
  AudioStream *p;  
  for(;;) {
    p->update_all();    
  }
}

void setup() {
    Serial.begin(115200);
    AudioMemory(40); 

    waveform1.amplitude(0.7);
    waveform1.frequency(4.0);
    waveform1.offset(0.3);
    //waveform1.pulseWidth(0.8);
    waveform1.begin(WAVEFORM_SINE);    

    ESP_LOGI(TAG, "I2C");
    if(!ac101.begin(CODEC_SDA, CODEC_SCK)) 
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

  if(f1 != o_amplitude) {
      o_amplitude = f1;
      waveform1.amplitude(o_amplitude); // TC :)
      Serial.print("Amplitude = ");
      Serial.println(o_amplitude);
  }
  if(f2 != o_frequency) {
      o_frequency = f2;
      waveform1.frequency(o_frequency*10); // TC :)
      Serial.print("Frequency = ");
      Serial.println(o_frequency*10);
  }

  digitalWrite(2,led);
  led=!led;
  if(digitalRead(5)==LOW) {
    Serial.println("Footswitch pressed!");
  }
}

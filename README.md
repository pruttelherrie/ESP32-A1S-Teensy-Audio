# ESP32-A1S-TeensyAudio

For use with PlatformIO.
 
Hard work done by macaba: https://github.com/macaba/ESP32Audio.

Example code I/O configuration is for BlackStomp: https://github.com/hamuro80/blackstomp

Use the Teensy Audio Library GUI for creating algorithms: https://www.pjrc.com/teensy/gui/
(GUI is also included in this repo, with small modifications where necessary)

Tested/Working blocks so far:
* I2S input & output (AC101 codec)
* mixer
* amp
* delay
* delay_ext
* biquad (lowpass, highpass)

To get going: download, add to PlatformIO, copy an example from the examples folder to /src/main.cpp, upload & play!

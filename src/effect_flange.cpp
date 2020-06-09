/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Pete (El Supremo)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Arduino.h>
#include "effect_flange.h"
//#include "arm_math.h"

/******************************************************************/
//                A u d i o E f f e c t F l a n g e
// Written by Pete (El Supremo) Jan 2014
// 140529 - change to handle mono stream and change modify() to voices()
// 140207 - fix calculation of delay_rate_incr which is expressed as
//			a fraction of 2*PI
// 140207 - cosmetic fix to begin()
// 140219 - correct the calculation of "frac"

// circular addressing indices for left and right channels
//short AudioEffectFlange::l_circ_idx;
//short AudioEffectFlange::r_circ_idx;

//short * AudioEffectFlange::l_delayline = NULL;
//short * AudioEffectFlange::r_delayline = NULL;

// User-supplied offset for the delayed sample
// but start with passthru
//int AudioEffectFlange::delay_offset_idx = FLANGE_DELAY_PASSTHRU;
//int AudioEffectFlange::delay_length;

//int AudioEffectFlange::delay_depth;
//int AudioEffectFlange::delay_rate_incr;
//unsigned int AudioEffectFlange::l_delay_rate_index;
//unsigned int AudioEffectFlange::r_delay_rate_index;
// fails if the user provides unreasonable values but will
// coerce them and go ahead anyway. e.g. if the delay offset
// is >= CHORUS_DELAY_LENGTH, the code will force it to
// CHORUS_DELAY_LENGTH-1 and return false.
// delay_rate is the rate (in Hz) of the sine wave modulation
// delay_depth is the maximum variation around delay_offset
// i.e. the total offset is delay_offset + delay_depth * sin(delay_rate)

// For updating on-the-fly, 3 parameters
boolean AudioEffectFlange::voices(int delay_offset,int d_depth,float delay_rate)
{
  boolean all_ok = true;
  
  delay_depth = d_depth;

  delay_rate_incr =(delay_rate * 2147483648.0)/ AUDIO_SAMPLE_RATE_EXACT;
  
  delay_offset_idx = delay_offset;
  // Allow the passthru code to go through
  if(delay_offset_idx < -1) {
    delay_offset_idx = 0;
    all_ok = false;
  }
  if(delay_offset_idx >= delay_length) {
    delay_offset_idx = delay_length - 1;
    all_ok = false;
  }
  l_delay_rate_phase = 0;
  l_circ_idx = 0;
  return(all_ok);
}


boolean AudioEffectFlange::begin(float *delayline,int d_length,int delay_offset,int d_depth,float delay_rate)
{
  boolean all_ok = true;

  delay_length = d_length;///2; 
  l_delayline = delayline;
  
  delay_depth = d_depth;
  // initial LFO phase
  l_delay_rate_phase = 0;
  l_circ_idx = 0;
  //delay_rate_incr =(delay_rate * 2147483648.0)/ AUDIO_SAMPLE_RATE_EXACT;
  delay_rate_incr = (float)(( 2.0 * PI ) / ( (float)AUDIO_SAMPLE_RATE_EXACT * 1.0 ));
  
  delay_offset_idx = delay_offset;
  // Allow the passthru code to go through
  if(delay_offset_idx < -1) {
    delay_offset_idx = 0;
    all_ok = false;
  }
  if(delay_offset_idx >= delay_length) {
    delay_offset_idx = delay_length - 1;
    all_ok = false;
  }  
  return(all_ok);
}


void IRAM_ATTR AudioEffectFlange::update(void)
{
  audio_block_t *block;
  int idx;
  float *bp;
  float frac;
  //int idx1;

  if(l_delayline == NULL) return;
/*
  // do passthru
  if(delay_offset_idx == FLANGE_DELAY_PASSTHRU) {
    // Just passthrough
    block = receiveWritable(0);
    if(block) {
      bp = block->data;
      // fill the delay line
      for(int i = 0;i < AUDIO_BLOCK_SAMPLES;i++) {
        l_circ_idx++;
        if(l_circ_idx >= delay_length) {
          l_circ_idx = 0;
        }
        l_delayline[l_circ_idx] = *bp++;
      }
      // transmit the unmodified block
      transmit(block,0);
      release(block);
    }
    return;
  }
*/
  //          L E F T  C H A N N E L

  block = receiveWritable(0);
  if(block) {
    bp = block->data;
    for(int i = 0;i < AUDIO_BLOCK_SAMPLES;i++) {
      // increment the index into the circular delay line buffer
      l_circ_idx++;
      // wrap the index around if necessary
      if(l_circ_idx >= delay_length) {
        l_circ_idx = 0;
      }
      // store the current sample in the delay line
      l_delayline[l_circ_idx] = *bp;

      // get sine of LFO phase, multiply by the delay depth. this is the
      // amount of 'wobble' around the delay center      
      frac = sin( l_delay_rate_phase ) * delay_depth;

      // first calculate the closest index
      idx = floor(frac);      
      // Calculate the offset into the buffer
      idx = l_circ_idx - (delay_offset_idx + idx);
      // and adjust idx to point into the circular buffer
      if(idx < 0) {
        idx += delay_length;
      }
      if(idx >= delay_length) {
        idx -= delay_length;
      }

/*
      // Here we interpolate between two indices but if the sine was negative
      // then we interpolate between idx and idx-1, otherwise the
      // interpolation is between idx and idx+1
      if(frac < 0)
        idx1 = idx - 1;
      else
        idx1 = idx + 1;
      // adjust idx1 in the circular buffer
      if(idx1 < 0) {
        idx1 += delay_length;
      }
      if(idx1 >= delay_length) {
        idx1 -= delay_length;
      }
      // Do the interpolation
      frac = (l_delay_rate_index >> 1) &0x7fff;
      frac = (( (int)(l_delayline[idx1] - l_delayline[idx])*frac) >> 15);
      *bp++ = (l_delayline[l_circ_idx]+ l_delayline[idx] + frac)/2;
*/
      *bp++ = ( l_delayline[l_circ_idx] + l_delayline[idx] ) / 2;

      // update LFO phase
      l_delay_rate_phase += delay_rate_incr;

      // and keep phase < 2*pi
      if(l_delay_rate_phase > (2 * PI)) l_delay_rate_phase -= (2 * PI);
    }
    // send the effect output to the left channel
    transmit(block,0);
    release(block);
  }
}




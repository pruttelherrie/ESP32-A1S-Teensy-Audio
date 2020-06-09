/* Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * 2. If the Software is incorporated into a build system that allows
 * selection among a list of target devices, then similar target
 * devices manufactured by PJRC.COM must be included in the list of
 * target devices and selectable in the same manner.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "AudioStream.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "Arduino.h"

#define MAX_AUDIO_MEMORY 163840
#define NUM_MASKS  (((MAX_AUDIO_MEMORY / AUDIO_BLOCK_SAMPLES / 2) + 31) / 32)

audio_block_t * AudioStream::memory_pool;
uint32_t AudioStream::memory_pool_available_mask[NUM_MASKS];
uint16_t AudioStream::memory_pool_first_mask;

//uint16_t AudioStream::cpu_cycles_total = 0;
//uint16_t AudioStream::cpu_cycles_total_max = 0;
uint16_t AudioStream::memory_used = 0;
uint16_t AudioStream::memory_used_max = 0;

// Set up the pool of audio data blocks
// placing them all onto the free list
void AudioStream::initialize_memory(audio_block_t *data, unsigned int num)
{
	unsigned int i;
	unsigned int maxnum = MAX_AUDIO_MEMORY / AUDIO_BLOCK_SAMPLES / 2;

	if (num > maxnum) num = maxnum;
	memory_pool = data;
	memory_pool_first_mask = 0;
	for (i=0; i < NUM_MASKS; i++) {
		memory_pool_available_mask[i] = 0;
	}
	for (i=0; i < num; i++) {
		memory_pool_available_mask[i >> 5] |= (1 << (i & 0x1F));
	}
	for (i=0; i < num; i++) {
		data[i].memory_pool_index = i;
	}
	//__disable_irq();

}

// Allocate 1 audio data block.  If successful
// the caller is the only owner of this new block
audio_block_t * AudioStream::allocate(void)
{
	uint32_t n, index, avail;
	uint32_t *p, *end;
	audio_block_t *block;
	uint32_t used;

	p = memory_pool_available_mask;
	end = p + NUM_MASKS;
	//__disable_irq();
	index = memory_pool_first_mask;
	p += index;
	while (1) {
		if (p >= end) {
			//__disable_irq();
			//Serial.println("alloc:null");
			return NULL;
		}
		avail = *p;
		if (avail) break;
		index++;
		p++;
	}
	n = __builtin_clz(avail);
	avail &= ~(0x80000000 >> n);
	*p = avail;
	if (!avail) index++;
	memory_pool_first_mask = index;
	used = memory_used + 1;
	memory_used = used;
	//__disable_irq();
	index = p - memory_pool_available_mask;
	block = memory_pool + ((index << 5) + (31 - n));
	block->ref_count = 1;
	if (used > memory_used_max) memory_used_max = used;
	//Serial.print("alloc:");
	//Serial.println((uint32_t)block, HEX);
	return block;
}

// Release ownership of a data block.  If no
// other streams have ownership, the block is
// returned to the free pool
void AudioStream::release(audio_block_t *block)
{
	//if (block == NULL) return;
	uint32_t mask = (0x80000000 >> (31 - (block->memory_pool_index & 0x1F)));
	uint32_t index = block->memory_pool_index >> 5;

	//__disable_irq();
	if (block->ref_count > 1) {
		block->ref_count--;
	} else {
		//Serial.print("reles:");
		//Serial.println((uint32_t)block, HEX);
		memory_pool_available_mask[index] |= mask;
		if (index < memory_pool_first_mask) memory_pool_first_mask = index;
		memory_used--;
	}
	//__disable_irq();
}

// Transmit an audio data block
// to all streams that connect to an output.  The block
// becomes owned by all the recepients, but also is still
// owned by this object.  Normally, a block must be released
// by the caller after it's transmitted.  This allows the
// caller to transmit to same block to more than 1 output,
// and then release it once after all transmit calls.
void AudioStream::transmit(audio_block_t *block, unsigned char index)
{
	for (AudioConnection *c = destination_list; c != NULL; c = c->next_dest) {
		if (c->src_index == index) {
			if (c->dst.inputQueue[c->dest_index] == NULL) {
				c->dst.inputQueue[c->dest_index] = block;
				block->ref_count++;
			}
		}
	}
}


// Receive block from an input.  The block's data
// may be shared with other streams, so it must not be written
audio_block_t * AudioStream::receiveReadOnly(unsigned int index)
{
	audio_block_t *in;

	if (index >= num_inputs) return NULL;
	in = inputQueue[index];
	inputQueue[index] = NULL;
	return in;
}

// Receive block from an input.  The block will not
// be shared, so its contents may be changed.
audio_block_t * AudioStream::receiveWritable(unsigned int index)
{
	audio_block_t *in, *p;

	if (index >= num_inputs) return NULL;
	in = inputQueue[index];
	inputQueue[index] = NULL;
	if (in && in->ref_count > 1) {
		p = allocate();
		if (p) memcpy(p->data, in->data, sizeof(p->data));
		in->ref_count--;
		in = p;
	}
	return in;
}


void AudioConnection::connect(void)
{
	AudioConnection *p;

	if (isConnected) return;
	if (dest_index > dst.num_inputs) return;
	//__disable_irq();
	p = src.destination_list;
	if (p == NULL) {
		src.destination_list = this;
	} else {
		while (p->next_dest) {
			if (&p->src == &this->src && &p->dst == &this->dst
				&& p->src_index == this->src_index && p->dest_index == this->dest_index) {
				//Source and destination already connected through another connection, abort
				//__disable_irq();
				return;
			}
			p = p->next_dest;
		}
		p->next_dest = this;
	}
	this->next_dest = NULL;
	src.numConnections++;
	src.active = true;

	dst.numConnections++;
	dst.active = true;

	isConnected = true;

	//__disable_irq();
}

void AudioConnection::disconnect(void)
{
	AudioConnection *p;

	if (!isConnected) return;
	if (dest_index > dst.num_inputs) return;
	//__disable_irq();
	// Remove destination from source list
	p = src.destination_list;
	if (p == NULL) {
		return;
	} else if (p == this) {
		if (p->next_dest) {
			src.destination_list = next_dest;
		} else {
			src.destination_list = NULL;
		}
	} else {
		while (p) {
			if (p == this) {
				if (p->next_dest) {
					p = next_dest;
					break;
				} else {
					p = NULL;
					break;
				}
			}
			p = p->next_dest;
		}
	}
	//Remove possible pending src block from destination
	dst.inputQueue[dest_index] = NULL;

	//Check if the disconnected AudioStream objects should still be active
	src.numConnections--;
	if (src.numConnections == 0) {
		src.active = false;
	}

	dst.numConnections--;
	if (dst.numConnections == 0) {
		dst.active = false;
	}

	isConnected = false;

	//__disable_irq();
}

bool AudioStream::blockingObjectRunning = false;

AudioStream * AudioStream::first_update = NULL;
const int updatesPerSecond = AUDIO_SAMPLE_RATE_EXACT / AUDIO_BLOCK_SAMPLES;
int updatePerSecondCounter = 0;

void AudioStream::update_all(void) // AudioStream::update_all()
{
	AudioStream *p;
	//portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
	for (p = AudioStream::first_update; p; p = p->next_update) {
		if (p->active) {
			if(p->blocking || !p->initialised){
				p->update();
				p->clocksPerUpdate = 0;
				// esp_task_wdt_reset();
			}
			else{
				//portENTER_CRITICAL(&mux);
				uint32_t startTick = xthal_get_ccount();
				p->update();
				uint32_t finishTick = xthal_get_ccount();
				//portEXIT_CRITICAL(&mux);
					
				if(finishTick > startTick)		//Ignore the wraparound boundary
				{
					p->clocksPerUpdate = (finishTick - startTick) - 11;			//11 is the minimum clocks to get the timer values
					p->clocksPerSecondSum += p->clocksPerUpdate;
					if(p->clocksPerUpdate > p->clocksPerUpdateMax)	
						p->clocksPerUpdateMax = p->clocksPerUpdate;
					if(p->clocksPerUpdate < p->clocksPerUpdateMin)
						p->clocksPerUpdateMin = p->clocksPerUpdate;
					if(updatePerSecondCounter == (updatesPerSecond - 1))
					{
						p->clocksPerUpdateMax = p->clocksPerUpdate;			//Reset max
						p->clocksPerUpdateMin = p->clocksPerUpdate;			//Reset min
						p->clocksPerSecond = p->clocksPerSecondSum;
						p->clocksPerSecondSum = 0;							//Reset sum
					}
				}
			}			
		}
	}
	updatePerSecondCounter++;
	if(updatePerSecondCounter == updatesPerSecond)
		updatePerSecondCounter = 0;
  //printf("%d",updatePerSecondCounter);
	if(!p->blockingObjectRunning)
		vTaskDelay(1000/portTICK_PERIOD_MS);		//If user is calling update_all but there aren't any streams controlling the timing, this'll stop 100% CPU

}
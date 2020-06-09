#include "input_i2s.h"
#include "Arduino.h"

void IRAM_ATTR AudioInputI2S::update(void)
{
	audio_block_t *new_left=NULL, *new_right=NULL;

	if(AudioControlI2S::configured)
	{
		new_left = allocate();
		if (new_left != NULL) {
			new_right = allocate();
			if (new_right == NULL) {
				release(new_left);
				new_left = NULL;
			}
		}

		//Serial.println("update");

		size_t bytesRead = 0;
		//i2s_adc_enable(I2S_NUM_0);
		switch(AudioControlI2S::bits)
		{
			case 16:
				i2s_read(I2S_NUM_0, (char*)&inputSampleBuffer, (AUDIO_BLOCK_SAMPLES * sizeof(uint32_t)), &bytesRead, portMAX_DELAY);		//Block but yield to other tasks
				break;
			case 24:
			case 32:
				i2s_read(I2S_NUM_0, (char*)&inputSampleBuffer, (AUDIO_BLOCK_SAMPLES * sizeof(uint32_t)) * 2, &bytesRead, portMAX_DELAY);		//Block but yield to other tasks
				break;
			default:
				printf("Unknown bit depth\n");
				break;
		}
		union foo input;
		union foo output;
		switch(AudioControlI2S::bits)
		{
			case 16:
				for(int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
				{
					if(new_left != NULL)
					{
						new_left->data[i] = ((float)(inputSampleBuffer[i] & 0xfff)/2048.0f) - 1.0f;
					}
				}
				break;
			case 24:
				for(int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
				{
					if(new_left != NULL)
					{
						input.integer = inputSampleBuffer[i*2];
						output.byte[0] = input.byte[1];
						output.byte[1] = input.byte[2];
						output.byte[2] = input.byte[3];
						if(CHECK_BIT(output.byte[2], 7))
						{
							output.byte[3] = 0xFF;
						}
						else
							output.byte[3] = 0x00;
						new_left->data[i] = ((float)output.integer)/8388608.0f;
					}
					if(new_right != NULL)
					{
						input.integer = inputSampleBuffer[i*2 + 1];
						output.byte[0] = input.byte[1];
						output.byte[1] = input.byte[2];
						output.byte[2] = input.byte[3];
						if(CHECK_BIT(output.byte[2], 7))
						{
							output.byte[3] = 0xFF;
						}
						else
							output.byte[3] = 0x00;
						new_right->data[i] = ((float)output.integer)/8388608.0f;
					}
				}
				break;
			case 32:
				for(int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
				{
					if(new_left != NULL)
					{
						new_left->data[i] = ((float)inputSampleBuffer[i*2])/1073741823.0f;
					}
					if(new_right != NULL)
					{
						new_right->data[i] = ((float)inputSampleBuffer[i*2+1])/1073741823.0f;
					}
				}
				break;
		}
		transmit(new_left, 0);
		release(new_left);
		transmit(new_right, 1);
		release(new_right);		
	}
}
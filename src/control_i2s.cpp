#include "control_i2s.h"
#include "driver/i2s.h"
#include "control_ac101.h"
#include "Arduino.h"

#define I2S_BCK_IO      (GPIO_NUM_27)
#define I2S_WS_IO       (GPIO_NUM_26)
#define I2S_DO_IO       (GPIO_NUM_25)
#define I2S_DI_IO       (GPIO_NUM_35)

//dma buffer length 32 bytes (8 samples: 4L+4R)
#define DMABUFFERLENGTH 32
//dma buffer count 20 (640 Bytes: 160 samples: 80L+80R) 
#define DMABUFFERCOUNT  20

static const char *TAG = "AudioControlI2S";

bool AudioControlI2S::configured = false;
uint8_t AudioControlI2S::bits = 32; // 16?!?

void AudioControlI2S::start(i2s_port_t i2s_port, i2s_config_t* i2s_config, i2s_pin_config_t* i2s_pin_config, bool outputMCLK)
{
    if(!configured)
    {
        bits = (uint8_t)i2s_config->bits_per_sample;

		//printf("+%d %p\n", 0, i2s_config);
		if (i2s_driver_install(i2s_port, i2s_config, 0, NULL) != ESP_OK) 
		{
			printf("ERROR: Unable to install I2S driver\n");
		} else {
            ESP_LOGI(TAG, "I2S Driver installed.");
        }

        if(i2s_pin_config != NULL)
        {
            i2s_set_pin(i2s_port, i2s_pin_config);
            ESP_LOGI(TAG, "I2S Pin Config set.");
        }

        if(outputMCLK)
        {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            REG_SET_FIELD(PIN_CTRL, CLK_OUT1, 0);
            ESP_LOGI(TAG, "MCLK output on CLK_OUT1.");
        }

        if(i2s_config->mode & I2S_MODE_DAC_BUILT_IN)
        {
            //i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
            ESP_LOGI(TAG, "I2S internal DAC set. (warning: not implemented)");
        }

        if(i2s_config->mode & I2S_MODE_ADC_BUILT_IN)
        {
            i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_6);		//Pin 34
	        i2s_adc_enable(I2S_NUM_0);
            ESP_LOGI(TAG, "I2S internal ADC set.");
        }
		configured = true;
	}
}

void AudioControlI2S::default_codec_rx_tx_24bit()
{
    i2s_mode_t mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    i2s_config_t i2s_config = {
        .mode = mode,                               
        .sample_rate = AUDIO_SAMPLE_RATE_EXACT,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                               //lowest interrupt priority
        .dma_buf_count = 2,
        .dma_buf_len = AUDIO_BLOCK_SAMPLES,
        .use_apll = 1,
        .tx_desc_auto_clear = true,
        .fixed_mclk = MCLK
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = 23                                                       //Not used
    };
    start((i2s_port_t)0, &i2s_config, &pin_config, true);
}

void AudioControlI2S::default_adc_dac()
{
    i2s_mode_t mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX | I2S_MODE_DAC_BUILT_IN | I2S_MODE_ADC_BUILT_IN);
    i2s_config_t i2s_config = {
    	.mode = mode,
    	.sample_rate = AUDIO_SAMPLE_RATE_EXACT,
    	.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    	.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    	.communication_format = I2S_COMM_FORMAT_I2S_MSB,
    	.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, 
    	.dma_buf_count = 2,
    	.dma_buf_len = AUDIO_BLOCK_SAMPLES,
    	.use_apll = 0,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    start((i2s_port_t)0, &i2s_config, NULL, false);
}

void AudioControlI2S::ac101()
{
 
  i2s_config_t i2s_config;
  i2s_config.mode =(i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  i2s_config.sample_rate = AUDIO_SAMPLE_RATE_EXACT;
  i2s_config.bits_per_sample = (i2s_bits_per_sample_t) 32; 
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
  i2s_config.dma_buf_count = DMABUFFERCOUNT;
  i2s_config.dma_buf_len = DMABUFFERLENGTH;
  i2s_config.use_apll = false;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  
  i2s_pin_config_t pin_config;
  pin_config.bck_io_num = I2S_BCK_IO;
  pin_config.ws_io_num = I2S_WS_IO;
  pin_config.data_out_num = I2S_DO_IO;
  pin_config.data_in_num = I2S_DI_IO;

  configured=true;
  ESP_LOGI(TAG, "Starting AC101 I2S config");
//  start((i2s_port_t)0, &i2s_config, &pin_config, false);
  if(i2s_driver_install((i2s_port_t)I2S_NUM, &i2s_config, 0, NULL) != ESP_OK)
    { ESP_LOGE(TAG,"i2s driver install error"); configured=false; }
  if(i2s_set_pin((i2s_port_t)I2S_NUM, &pin_config) != ESP_OK)
    { ESP_LOGE(TAG,"i2s set pin error"); configured=false; }
  if(i2s_set_clk((i2s_port_t)I2S_NUM, AUDIO_SAMPLE_RATE_EXACT, (i2s_bits_per_sample_t) 32, I2S_CHANNEL_STEREO) != ESP_OK)
    { ESP_LOGE(TAG,"i2s set clk error"); configured=false; }

  if(configured) {
    ESP_LOGI(TAG, "AudioControlI2S configured");
  } else {
    ESP_LOGE(TAG, "Failed to configure AudioControlI2S");
  }
}

AudioControlI2S::~AudioControlI2S(){
    if(configured)
    {
        ESP_LOGI(TAG, "Uninstall I2S.");
        i2s_driver_uninstall((i2s_port_t)0); //stop & destroy i2s driver
    }
}
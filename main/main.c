
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/dac_continuous.h"
#include "esp_check.h"
#include "soundData.h"


#define LED 2
#define BUTTON 15
#define ON 1
#define OFF 0

int toggle = 0;

QueueHandle_t interruptQueue;
dac_continuous_handle_t dac_handle;

/* function to play the sound file sample */
static void play_sound(int status) {
	if (status) {
		while (1) {
			printf("Turning on sound...\n");
			ESP_ERROR_CHECK(dac_continuous_write(dac_handle, (uint8_t *) sample, sample_len, NULL, -1));
		}
	} else {
		ESP_ERROR_CHECK(dac_continuous_disable(dac_handle));
	}
}

static void IRAM_ATTR gpio_interupt_handler(void *args) {
	int tmp = (int) args;
	xQueueSendFromISR(interruptQueue, &tmp, NULL);
}

/* turn on sound file and status LED */
void turn_on_sound() {
	/* turn on sound file */
	play_sound(ON);

	/* turn on LED */
	gpio_set_level(LED, 1);
}

/* turn off sound file and status LED */
void turn_off_sound() {
	/* turn off sound file */
	play_sound(OFF);

	/* turn off LED */
	gpio_set_level(LED, 0);
}

/* initialize hardware */
void hw_init() {
	/* setup DAC config */
	dac_continuous_config_t dac_cfg = {
	        .chan_mask = DAC_CHANNEL_MASK_CH0,
	        .desc_num = 8,
	        .buf_size = 2048,
	        .freq_hz = 16000,
	        .offset = 0,
	        .clk_src = DAC_DIGI_CLK_SRC_APLL,   // Using APLL as clock source to get a wider frequency range
	        /* Assume the data in buffer is 'A B C D E F'
	         * DAC_CHANNEL_MODE_SIMUL:
	         *      - channel 0: A B C D E F
	         *      - channel 1: A B C D E F
	         * DAC_CHANNEL_MODE_ALTER:
	         *      - channel 0: A C E
	         *      - channel 1: B D F
	         */
	        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
	    };

	/* setup DAC for sound output */
	ESP_ERROR_CHECK(dac_continuous_new_channels(&dac_cfg, &dac_handle));
	ESP_ERROR_CHECK(dac_continuous_enable(dac_handle));

	/* setup button */
	gpio_reset_pin(BUTTON);
	gpio_set_direction(BUTTON, GPIO_MODE_INPUT);
	gpio_set_intr_type(BUTTON, GPIO_INTR_POSEDGE);

	/* setup status LED */
	gpio_reset_pin(LED);
	gpio_set_direction(LED, GPIO_MODE_OUTPUT);

	/* turn everything off at startup */
	turn_off_sound();
}

/* monitor the toggle button */
void button_monitor() {
	int tmp;

	while (true) {
		if (xQueueReceive(interruptQueue, &tmp, portMAX_DELAY)) {
			if (toggle == 0) {
				turn_on_sound();
				toggle = 1;
				printf("LED on\n");
			} else {
				turn_off_sound();
				toggle = 0;
				printf("LED off\n");
			}

			/* no need to respond if its been less than one second since the last "toggle" */
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			xQueueReset(interruptQueue);
		}
	}
}

void app_main(void) {
	/* initialize hardware */
	hw_init();

	interruptQueue = xQueueCreate(10, sizeof(int));
	xTaskCreate(button_monitor, "button_monitor", 2048, NULL, 1, NULL);

	/* default ISR service configuration */
	gpio_install_isr_service(0);

	/* attach the ISR */
	gpio_isr_handler_add(BUTTON, gpio_interupt_handler, (void *) BUTTON);
}

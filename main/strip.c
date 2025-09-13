#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "led_strip.h"

#include "strip.h"

enum StripMode gStripMode = STRIP_MODE_RGB;
uint32_t gStripData[STRIP_LED_COUNT][3];

static led_strip_handle_t sStrip;

static void strip_configure(void) {
	led_strip_config_t stripCfg = {
		.strip_gpio_num = STRIP_PIN,
		.led_model = STRIP_LED_MODEL,
		.color_component_format = STRIP_LED_COLOR_FORMAT,
		.max_leds = STRIP_LED_COUNT,
	};

	led_strip_rmt_config_t rmtCfg = {
		.flags.with_dma = true
	};

	led_strip_new_rmt_device(&stripCfg, &rmtCfg, &sStrip);
	led_strip_clear(sStrip);
}

static void strip_update(void) {
	for (size_t i = 0; i < STRIP_LED_COUNT; i++) {
		switch (gStripMode) {
			case STRIP_MODE_RGB:
				led_strip_set_pixel(
					sStrip,
					i,
					gStripData[i][0] % 256,
					gStripData[i][1] % 256,
					gStripData[i][2] % 256);
				break;

			case STRIP_MODE_HSV:
				led_strip_set_pixel_hsv(
					sStrip,
					i,
					gStripData[i][0] % 360,
					gStripData[i][1] % 256,
					gStripData[i][2] % 256);
				break;
		}
	}

	led_strip_refresh(sStrip);
}

void strip_task(void *pvParameters) {
	strip_configure();

	while (true) {
		strip_update();
	}
}

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "bytecode.h"
#include "server.h"
#include "strip.h"

void app_main(void) {
	strip_init();
	strip_start();

	bc_init();
	bc_start();

	server_init();
	server_start();

/*
	uint32_t time = 0;

	while (1) {
		gStripMode = STRIP_MODE_RGB;

		for (size_t i = 0; i < STRIP_LED_COUNT; i++) {
			uint32_t pos = ((STRIP_LED_COUNT - i + time) / 3) % 4;

			switch (pos) {
				case 0:
					gStripData[i][0] = 91;
					gStripData[i][1] = 206;
					gStripData[i][2] = 250;
					break;

				case 1:
				case 3:
					gStripData[i][0] = 245;
					gStripData[i][1] = 169;
					gStripData[i][2] = 184;
					break;

				case 2:
					gStripData[i][0] = 255;
					gStripData[i][1] = 255;
					gStripData[i][2] = 255;
					break;
			}
		}

		time++;
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
//*/
}

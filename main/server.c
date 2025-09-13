#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "strip.h"
#include "server.h"

void server_task(void *pvParameters) {
	uint32_t time = 0;

	while (1) {

/*

		gStripMode = STRIP_MODE_HSV;

		for (size_t i = 0; i < STRIP_LED_COUNT; i++) {
			gStripData[i][0] = time + (STRIP_LED_COUNT - i) * 10;
			gStripData[i][1] = 255;
			gStripData[i][2] = 255;
		}

/*/

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

//*/

		time++;
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

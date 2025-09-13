#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "server.h"
#include "strip.h"

void app_main(void) {
	xTaskCreatePinnedToCore(
		&server_task,
		"server_task",
		SERVER_TASK_STACK_SIZE_BYTES,
		NULL,
		SERVER_TASK_PRIORITY,
		NULL,
		SERVER_TASK_CORE);

	xTaskCreatePinnedToCore(
		&strip_task,
		"strip_task",
		STRIP_TASK_STACK_SIZE_BYTES,
		NULL,
		STRIP_TASK_PRIORITY,
		NULL,
		STRIP_TASK_CORE);
}

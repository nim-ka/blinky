#pragma once

#define STRIP_PIN GPIO_NUM_2
#define STRIP_LED_MODEL LED_MODEL_WS2812
#define STRIP_LED_COLOR_FORMAT LED_STRIP_COLOR_COMPONENT_FMT_GRB
#define STRIP_LED_COUNT 300

#define STRIP_TASK_STACK_SIZE_BYTES 0x4000
#define STRIP_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define STRIP_TASK_CORE 1

enum StripMode {
	STRIP_MODE_RGB,
	STRIP_MODE_HSV
};

extern enum StripMode gStripMode;
extern uint32_t gStripData[STRIP_LED_COUNT][3];

extern void strip_init(void);
extern void strip_start(void);
extern void strip_suspend(void);
extern void strip_resume(void);

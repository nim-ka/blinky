#pragma once

#define SERVER_TASK_STACK_SIZE_BYTES 0x4000
#define SERVER_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define SERVER_TASK_CORE 0

extern void server_init(void);
extern void server_start(void);

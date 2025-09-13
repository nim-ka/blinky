#pragma once

#define BC_MAX_LEN 0x4000

#define BC_TASK_STACK_SIZE_BYTES 0x4000
#define BC_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define BC_TASK_CORE 0

struct BytecodeOp {
	uint32_t arity;
	void (*func)(uint8_t *args);
};

extern uint8_t gBytecode[BC_MAX_LEN];
extern size_t gBytecodeLen;

extern void bc_init(void);
extern void bc_start(void);
extern bool bc_try_update(uint8_t *bytecode);

#pragma once

#define BC_MAX_LEN 0x4000
#define BC_MAX_INSTRS 100000
#define BC_ERR_PATTERN_SIZE 18
#define BC_MEMORY_SIZE 0x1000

#define BC_TASK_STACK_SIZE_BYTES 0x4000
#define BC_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define BC_TASK_CORE 0

struct ErrorBytecode {
	uint8_t crc;
	uint8_t pattern[BC_ERR_PATTERN_SIZE];
	char message[256];
	uint8_t end[8];
};

struct BytecodeOp {
	uint32_t arity;
	void (*func)(uint8_t *args);
};

extern uint8_t gBytecode[BC_MAX_LEN];
extern size_t gBytecodeLen;

extern void bc_init(void);
extern void bc_start(void);
extern bool bc_update(uint8_t *bytecode, bool checkCrc);
extern void bc_interrupt(void);

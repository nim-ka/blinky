#pragma once

#define BYTECODE_MAX_LEN 0x4000

extern uint8_t gBytecode[BYTECODE_MAX_LEN];
extern size_t gBytecodeLen;

extern void bytecode_init(void);
extern bool bytecode_try_update(uint8_t *bytecode);

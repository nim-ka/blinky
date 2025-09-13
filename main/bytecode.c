#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "strip.h"
#include "bytecode.h"

#include <stdio.h>

uint8_t gBytecode[BYTECODE_MAX_LEN];
size_t gBytecodeLen;

static uint8_t bytecode_crc(uint8_t *bytecode, size_t len) {
	uint8_t crc = 0;

	for (size_t i = 1; i < len; i++) {
		crc ^= bytecode[i];

		for (uint32_t j = 0; j < 8; j++) {
			if (crc & 0x80) {
				crc <<= 1;
				crc ^= 0x31;
			} else {
				crc <<= 1;
			}
		}
	}

	return crc;
}

static size_t bytecode_len(uint8_t *bytecode) {
	size_t len = 8;

	while (len < BYTECODE_MAX_LEN) {
		if (
			bytecode[len - 8] == 0xFF &&
			bytecode[len - 7] == 0xFF &&
			bytecode[len - 6] == 0xFF &&
			bytecode[len - 5] == 0xFF &&
			bytecode[len - 4] == 0xFF &&
			bytecode[len - 3] == 0xFF &&
			bytecode[len - 2] == 0xFF &&
			bytecode[len - 1] == 0xFF) {
			break;
		}

		len++;
	}

	return len;
}

void bytecode_init(void) {
	for (size_t i = 0; i < 8; i++) {
		gBytecode[1 + i] = 0xFF;
	}

	gBytecodeLen = bytecode_len(gBytecode);
	gBytecode[0] = bytecode_crc(gBytecode, gBytecodeLen);
}

bool bytecode_try_update(uint8_t *bytecode) {
	size_t len = bytecode_len(bytecode);

	uint8_t crc = bytecode_crc(bytecode, len);
	if (bytecode[0] != crc) {
		return false;
	}

	strip_suspend();
	memcpy(gBytecode, bytecode, len);
	gBytecodeLen = len;
	strip_resume();

	return true;
}

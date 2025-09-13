#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"

#include "strip.h"
#include "bytecode.h"

uint8_t gBytecode[BC_MAX_LEN];
size_t gBytecodeLen;

static TaskHandle_t sBytecodeTask;

static uint32_t sTicks;
static uint32_t sPeriodMs;

static size_t sCurLed;
static size_t sPc;
static bool sRunning;

static int32_t sRegisters[256];
static bool sCompare;

static inline uint8_t bc_next_u8(void) {
	return gBytecode[sPc++];
}

static inline int32_t bc_next_s32(void) {
	int32_t n = gBytecode[sPc++];
	n <<= 8;
	n |= gBytecode[sPc++];
	n <<= 8;
	n |= gBytecode[sPc++];
	n <<= 8;
	n |= gBytecode[sPc++];

	return n;
}

static void bc_op_nop(void) {}

static void bc_op_rgb(void) {
	gStripMode = STRIP_MODE_RGB;
}

static void bc_op_hsv(void) {
	gStripMode = STRIP_MODE_HSV;
}

static void bc_op_periodi(void) {
	int32_t imm = bc_next_s32();
	sPeriodMs = imm;
}

static void bc_op_periodr(void) {
	uint8_t reg = bc_next_u8();
	sPeriodMs = sRegisters[reg];
}

static void bc_op_redi(void) {
	int32_t imm = bc_next_s32();
	gStripData[sCurLed][0] = imm;
}

static void bc_op_greeni(void) {
	int32_t imm = bc_next_s32();
	gStripData[sCurLed][1] = imm;
}

static void bc_op_bluei(void) {
	int32_t imm = bc_next_s32();
	gStripData[sCurLed][2] = imm;
}

static void bc_op_redr(void) {
	uint8_t reg = bc_next_u8();
	gStripData[sCurLed][0] = sRegisters[reg];
}

static void bc_op_greenr(void) {
	uint8_t reg = bc_next_u8();
	gStripData[sCurLed][1] = sRegisters[reg];
}

static void bc_op_bluer(void) {
	uint8_t reg = bc_next_u8();
	gStripData[sCurLed][2] = sRegisters[reg];
}

static void bc_op_getpos(void) {
	uint8_t reg = bc_next_u8();
	sRegisters[reg] = sCurLed;
}

static void bc_op_getticks(void) {
	uint8_t reg = bc_next_u8();
	sRegisters[reg] = sTicks;
}

static void bc_op_debug(void) {
	uint8_t reg = bc_next_u8();
	printf("pc %d: r%d = %ld\n", sPc, reg, sRegisters[reg]);
}

static void bc_op_movi(void) {
	uint8_t reg = bc_next_u8();
	int32_t imm = bc_next_s32();
	sRegisters[reg] = imm;
}

static void bc_op_movr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1];
}

static void bc_op_addi(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	int32_t imm = bc_next_s32();
	sRegisters[reg0] = sRegisters[reg1] + imm;
}

static void bc_op_addr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1] + sRegisters[reg2];
}

static void bc_op_subr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1] - sRegisters[reg2];
}

static void bc_op_muli(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	int32_t imm = bc_next_s32();
	sRegisters[reg0] = sRegisters[reg1] * imm;
}

static void bc_op_mulr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1] * sRegisters[reg2];
}

static void bc_op_divi(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	int32_t imm = bc_next_s32();
	sRegisters[reg0] = sRegisters[reg1] / imm;
}

static void bc_op_divr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1] / sRegisters[reg2];
}

static void bc_op_modi(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	int32_t imm = bc_next_s32();
	sRegisters[reg0] = sRegisters[reg1] % imm;
}

static void bc_op_modr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1] % sRegisters[reg2];
}

static void bc_op_goto(void) {
	int32_t dest = bc_next_s32();
	sPc = dest;
}

static void bc_op_jt(void) {
	int32_t dest = bc_next_s32();
	if (sCompare) {
		sPc = dest;
	}
}

static void bc_op_jf(void) {
	int32_t dest = bc_next_s32();
	if (!sCompare) {
		sPc = dest;
	}
}

static void bc_op_cz(void) {
	uint8_t reg = bc_next_u8();
	sCompare = sRegisters[reg] == 0;
}

static void bc_op_cnz(void) {
	uint8_t reg = bc_next_u8();
	sCompare = sRegisters[reg] != 0;
}

static void bc_op_ceqi(void) {
	uint8_t reg = bc_next_u8();
	int32_t imm = bc_next_s32();
	sCompare = sRegisters[reg] == imm;
}

static void bc_op_ceqr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] == sRegisters[reg1];
}

static void bc_op_clti(void) {
	uint8_t reg = bc_next_u8();
	int32_t imm = bc_next_s32();
	sCompare = sRegisters[reg] < imm;
}

static void bc_op_cltr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] < sRegisters[reg1];
}

static void bc_op_clei(void) {
	uint8_t reg = bc_next_u8();
	int32_t imm = bc_next_s32();
	sCompare = sRegisters[reg] <= imm;
}

static void bc_op_cler(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] <= sRegisters[reg1];
}

static void bc_op_cgti(void) {
	uint8_t reg = bc_next_u8();
	int32_t imm = bc_next_s32();
	sCompare = sRegisters[reg] > imm;
}

static void bc_op_cgtr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] > sRegisters[reg1];
}

static void bc_op_cgei(void) {
	uint8_t reg = bc_next_u8();
	int32_t imm = bc_next_s32();
	sCompare = sRegisters[reg] >= imm;
}

static void bc_op_cger(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] >= sRegisters[reg1];
}

static void bc_op_halt(void) {
	sRunning = false;
}

#define OP(opcode, func) bc_op_ ## func,
#define OP_ALIAS(opcode, func)
#define OP_NONE(opcode) NULL,

static void (*sOps[256])(void) = {
#include "files/ops.h"
};

#undef OP
#undef OP_NONE

static uint8_t bc_crc(uint8_t *bytecode, size_t len) {
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

static size_t bc_len(uint8_t *bytecode) {
	size_t len = 8;

	while (len < BC_MAX_LEN) {
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

uint32_t instrs;

static void bc_execute(void) {
	gStripMode = STRIP_MODE_RGB;
	sPeriodMs = 1000;
instrs = 0;

	for (sCurLed = 0; sCurLed < STRIP_LED_COUNT; sCurLed++) {
		sPc = 1;
		sRunning = true;

		gStripData[sCurLed][0] = 0;
		gStripData[sCurLed][1] = 0;
		gStripData[sCurLed][2] = 0;

		while (sRunning) {
			if (sPc >= gBytecodeLen) {
				sRunning = false;
				break;
			}

			uint8_t opcode = gBytecode[sPc];
			sPc++;

			if (sOps[opcode] == NULL) {
				sRunning = false;
				break;
			}

			sOps[opcode]();
instrs++;
		}
	}
}

static void bc_task(void *pvParameters) {
	while (true) {
uint32_t ts = xTaskGetTickCount();
		strip_suspend();
		bc_execute();
		strip_resume();
uint32_t te = xTaskGetTickCount();

		sTicks++;

		TickType_t delay = pdMS_TO_TICKS(sPeriodMs);
		xTaskNotifyWait(0, 0, NULL, delay < 1 ? 1 : delay);

//printf("bc tick %ld instrs %ld exec time %ld ms\n", sTicks, instrs, pdTICKS_TO_MS(te - ts));
	}
}

void bc_init(void) {
	sTicks = 0;
	sPeriodMs = 1000;

	for (size_t i = 0; i < 8; i++) {
		gBytecode[1 + i] = 0xFF;
	}

	gBytecodeLen = bc_len(gBytecode);
	gBytecode[0] = bc_crc(gBytecode, gBytecodeLen);
}

void bc_start(void) {
	xTaskCreatePinnedToCore(
		&bc_task,
		"bc_task",
		BC_TASK_STACK_SIZE_BYTES,
		NULL,
		BC_TASK_PRIORITY,
		&sBytecodeTask,
		BC_TASK_CORE);
}

bool bc_try_update(uint8_t *bytecode) {
	size_t len = bc_len(bytecode);

	uint8_t crc = bc_crc(bytecode, len);
	if (bytecode[0] != crc) {
//		return false;
	}

	sTicks = 0;

	memcpy(gBytecode, bytecode, len);
	gBytecodeLen = len;

	xTaskNotifyGive(sBytecodeTask);

	return true;
}

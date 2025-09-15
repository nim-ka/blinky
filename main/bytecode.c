#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"

#include "strip.h"
#include "bytecode.h"

#define ERROR(...) \
	{ \
		sError = true; \
		sRunning = false; \
		memset(sErrorBytecode.message, 0, sizeof(sErrorBytecode.message)); \
		sprintf(sErrorBytecode.message, __VA_ARGS__); \
		bc_update((uint8_t *) &sErrorBytecode, false); \
	}

uint8_t gBytecode[BC_MAX_LEN];
size_t gBytecodeLen;

static uint8_t sInitBytecode[76] = {
	/* checksum */ 0x00,
	/* mode */ BC_MODE_PER_LED,
	/* 02: hsv             */ 0x02,
	/* 03: periodi 100.0f  */ 0x03, 0x42, 0xC8, 0x00, 0x00,
	/* 08: vali 150.0f     */ 0x07, 0x43, 0x16, 0x00, 0x00,
	/* 0D: getposend r0    */ 0x0C, 0x00,
	/* 0F: getticks r1     */ 0x0D, 0x01,
	/* 11: addr r0 r0 r1   */ 0x13, 0x00, 0x00, 0x01,
	/* 15: divi r0 r0 3.0f */ 0x17, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00,
	/* 1C: modi r0 r0 4.0f */ 0x19, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00,
	/* 23: cz r0           */ 0x41, 0x00,
	/* 25: haltt           */ 0x33,
	/* 26: huei 348.0f     */ 0x05, 0x43, 0xAE, 0x00, 0x00,
	/* 2B: sati 79.0f      */ 0x06, 0x42, 0x9E, 0x00, 0x00,
	/* 30: modi r1 r0 2.0f */ 0x19, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00,
	/* 37: cnz r1          */ 0x42, 0x01,
	/* 39: haltt           */ 0x33,
	/* 3A: huei 197.0f     */ 0x05, 0x43, 0x45, 0x00, 0x00,
	/* 3F: sati 162.0f     */ 0x06, 0x43, 0x22, 0x00, 0x00,
	/* end */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static struct ErrorBytecode sErrorBytecode = {
	.mode = BC_MODE_PER_LED,
	.pattern = {
		/* 01: getticks r0     */ 0x0D, 0x00,
		/* 03: modi r0 r0 2.0f */ 0x19, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
		/* 0A: cz r0           */ 0x41, 0x00,
		/* 0C: haltf           */ 0x34,
		/* 0D: redi 255.0f     */ 0x05, 0x43, 0x7F, 0x00, 0x00,
		/* 12: halt            */ 0xFF
	},
	.end = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
};

static TaskHandle_t sBytecodeTask;

// Settings
static uint32_t sTicks;
static uint32_t sPeriodMs;

// Tracking
static uint32_t sInstrs;
static bool sError;
static bool sRunning;

// Program state
static float sRegisters[256];
static float sMemory[BC_MEMORY_SIZE];
static bool sCompare;
static size_t sCurLed;
static uint32_t sRng;
static size_t sPc;

static inline float bc_read_mem(size_t idx) {
	if (idx >= BC_MEMORY_SIZE) {
		ERROR("out of bounds memory read (addr %03x)", idx);
		return 0.0f;
	}

	return sMemory[idx];
}

static inline void bc_write_mem(size_t idx, float val) {
	if (idx >= BC_MEMORY_SIZE) {
		ERROR("out of bounds memory write (addr %04x)", idx);
		return;
	}

	sMemory[idx] = val;
}

static inline void bc_set_cur_led(size_t pos) {
	if (pos >= STRIP_LED_COUNT) {
		ERROR("tried to set led outside the strip (position %d)", pos);
		return;
	}

	sCurLed = pos;
}

static inline uint8_t bc_next_u8(void) {
	return gBytecode[sPc++];
}

static inline uint32_t bc_next_u32(void) {
	uint32_t n = gBytecode[sPc++];
	n <<= 8;
	n |= gBytecode[sPc++];
	n <<= 8;
	n |= gBytecode[sPc++];
	n <<= 8;
	n |= gBytecode[sPc++];

	return n;
}

static inline float bc_next_f32(void) {
	union {
		uint32_t i;
		float f;
	} cast = { .i = bc_next_u32() };

	return cast.f;
}

/* Nop instruction */

static void bc_op_nop(void) {}

/* Configuration instructions */

static void bc_op_rgb(void) {
	gStripMode = STRIP_MODE_RGB;
}

static void bc_op_hsv(void) {
	gStripMode = STRIP_MODE_HSV;
}

static void bc_op_periodi(void) {
	float imm = bc_next_f32();
	sPeriodMs = (uint32_t) imm;
}

static void bc_op_periodr(void) {
	uint8_t reg = bc_next_u8();
	sPeriodMs = (uint32_t) sRegisters[reg];
}

static void bc_op_redi(void) {
	float imm = bc_next_f32();
	gStripData[sCurLed][0] = (uint32_t) imm;
}

static void bc_op_greeni(void) {
	float imm = bc_next_f32();
	gStripData[sCurLed][1] = (uint32_t) imm;
}

static void bc_op_bluei(void) {
	float imm = bc_next_f32();
	gStripData[sCurLed][2] = (uint32_t) imm;
}

static void bc_op_redr(void) {
	uint8_t reg = bc_next_u8();
	gStripData[sCurLed][0] = (uint32_t) sRegisters[reg];
}

static void bc_op_greenr(void) {
	uint8_t reg = bc_next_u8();
	gStripData[sCurLed][1] = (uint32_t) sRegisters[reg];
}

static void bc_op_bluer(void) {
	uint8_t reg = bc_next_u8();
	gStripData[sCurLed][2] = (uint32_t) sRegisters[reg];
}

static void bc_op_getpos(void) {
	uint8_t reg = bc_next_u8();
	sRegisters[reg] = (float) sCurLed;
}

static void bc_op_getposend(void) {
	uint8_t reg = bc_next_u8();
	sRegisters[reg] = (float) (STRIP_LED_COUNT - sCurLed);
}

static void bc_op_getticks(void) {
	uint8_t reg = bc_next_u8();
	sRegisters[reg] = (float) sTicks;
}

static void bc_op_getrng(void) {
	uint8_t reg = bc_next_u8();
	sRegisters[reg] = (float) sRng / 0xFFFFFFFFU;
}

static void bc_op_getnumleds(void) {
	uint8_t reg = bc_next_u8();
	sRegisters[reg] = STRIP_LED_COUNT;
}

/* Arithmetic instructions */

static void bc_op_movi(void) {
	uint8_t reg = bc_next_u8();
	float imm = bc_next_f32();
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
	float imm = bc_next_f32();
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
	float imm = bc_next_f32();
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
	float imm = bc_next_f32();

	if (imm == 0.0f) {
		ERROR("divi by zero");
	} else {
		sRegisters[reg0] = sRegisters[reg1] / imm;
	}
}

static void bc_op_divr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();

	if (sRegisters[reg2] == 0.0f) {
		ERROR("divr by zero");
	} else {
		sRegisters[reg0] = sRegisters[reg1] / sRegisters[reg2];
	}
}

static void bc_op_modi(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	float imm = bc_next_f32();

	int32_t div = (int32_t) imm;
	if (div == 0) {
		ERROR("modi by zero");
	} else {
		sRegisters[reg0] = (float) ((int32_t) sRegisters[reg1] % div);
	}
}

static void bc_op_modr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();

	int32_t div = (int32_t) sRegisters[reg2];
	if (div == 0) {
		ERROR("modr by zero");
	} else {
		sRegisters[reg0] = (float) ((int32_t) sRegisters[reg1] % div);
	}
}

static void bc_op_remi(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	float imm = bc_next_f32();

	int32_t div = (int32_t) imm;
	if (div == 0) {
		ERROR("remi by zero");
	} else {
		sRegisters[reg0] = (float) (((int32_t) sRegisters[reg1] % div + div) % div);
	}
}

static void bc_op_remr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();

	int32_t div = (int32_t) sRegisters[reg2];
	if (div == 0) {
		ERROR("remr by zero");
	} else {
		sRegisters[reg0] = (float) (((int32_t) sRegisters[reg1] % div + div) % div);
	}
}

static void bc_op_sinr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = sinf(sRegisters[reg1]);
}

static void bc_op_cosr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = cosf(sRegisters[reg1]);
}

static void bc_op_tanr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = tanf(sRegisters[reg1]);
}

static void bc_op_asinr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = asinf(sRegisters[reg1]);
}

static void bc_op_acosr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = acosf(sRegisters[reg1]);
}

static void bc_op_atanr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = atanf(sRegisters[reg1]);
}

static void bc_op_atan2r(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();
	sRegisters[reg0] = atan2f(sRegisters[reg1], sRegisters[reg2]);
}

static void bc_op_sqrtr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = sqrtf(sRegisters[reg1]);
}

static void bc_op_floorr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = floorf(sRegisters[reg1]);
}

static void bc_op_ceilr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = ceilf(sRegisters[reg1]);
}

static void bc_op_roundr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = roundf(sRegisters[reg1]);
}

static void bc_op_mini(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	float imm = bc_next_f32();
	sRegisters[reg0] = sRegisters[reg1] < imm ? sRegisters[reg1] : imm;
}

static void bc_op_minr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1] < sRegisters[reg2] ? sRegisters[reg1] : sRegisters[reg2];
}

static void bc_op_maxi(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	float imm = bc_next_f32();
	sRegisters[reg0] = sRegisters[reg1] > imm ? sRegisters[reg1] : imm;
}

static void bc_op_maxr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	uint8_t reg2 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1] > sRegisters[reg2] ? sRegisters[reg1] : sRegisters[reg2];
}

static void bc_op_clampi(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	float imm0 = bc_next_f32();
	float imm1 = bc_next_f32();
	sRegisters[reg0] = sRegisters[reg1] < imm0 ? imm0 : sRegisters[reg1] > imm1 ? imm1 : sRegisters[reg1];
}

static void bc_op_absr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = sRegisters[reg1] < 0 ? -sRegisters[reg1] : sRegisters[1];
}

/* Control flow instructions */

static void bc_op_goto(void) {
	uint32_t dest = bc_next_u32();
	sPc = dest;
}

static void bc_op_jt(void) {
	uint32_t dest = bc_next_u32();
	if (sCompare) {
		sPc = dest;
	}
}

static void bc_op_jf(void) {
	uint32_t dest = bc_next_u32();
	if (!sCompare) {
		sPc = dest;
	}
}

static void bc_op_haltt(void) {
	if (sCompare) {
		sRunning = false;
	}
}

static void bc_op_haltf(void) {
	if (!sCompare) {
		sRunning = false;
	}
}

/* Comparison instructions */

static void bc_op_getcmp(void) {
	uint8_t reg = bc_next_u8();
	sRegisters[reg] = (float) sCompare;
}

static void bc_op_cz(void) {
	uint8_t reg = bc_next_u8();
	sCompare = sRegisters[reg] == 0.0f;
}

static void bc_op_cnz(void) {
	uint8_t reg = bc_next_u8();
	sCompare = sRegisters[reg] != 0.0f;
}

static void bc_op_ceqi(void) {
	uint8_t reg = bc_next_u8();
	float imm = bc_next_f32();
	sCompare = sRegisters[reg] == imm;
}

static void bc_op_ceqr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] == sRegisters[reg1];
}

static void bc_op_clti(void) {
	uint8_t reg = bc_next_u8();
	float imm = bc_next_f32();
	sCompare = sRegisters[reg] < imm;
}

static void bc_op_cltr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] < sRegisters[reg1];
}

static void bc_op_clei(void) {
	uint8_t reg = bc_next_u8();
	float imm = bc_next_f32();
	sCompare = sRegisters[reg] <= imm;
}

static void bc_op_cler(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] <= sRegisters[reg1];
}

static void bc_op_cgti(void) {
	uint8_t reg = bc_next_u8();
	float imm = bc_next_f32();
	sCompare = sRegisters[reg] > imm;
}

static void bc_op_cgtr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] > sRegisters[reg1];
}

static void bc_op_cgei(void) {
	uint8_t reg = bc_next_u8();
	float imm = bc_next_f32();
	sCompare = sRegisters[reg] >= imm;
}

static void bc_op_cger(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sCompare = sRegisters[reg0] >= sRegisters[reg1];
}

/* Memory instructions */

static void bc_op_loadi(void) {
	uint8_t reg = bc_next_u8();
	float imm = bc_next_f32();
	sRegisters[reg] = bc_read_mem((size_t) imm);
}

static void bc_op_loadr(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	sRegisters[reg0] = bc_read_mem((size_t) sRegisters[reg1]);
}

static void bc_op_storei(void) {
	uint8_t reg = bc_next_u8();
	float imm = bc_next_f32();
	bc_write_mem((size_t) imm, sRegisters[reg]);
}

static void bc_op_storer(void) {
	uint8_t reg0 = bc_next_u8();
	uint8_t reg1 = bc_next_u8();
	bc_write_mem((size_t) sRegisters[reg1], sRegisters[reg0]);
}

/* Configuration instructions 2 */

static void bc_op_posi(void) {
	float imm = bc_next_f32();
	bc_set_cur_led((size_t) imm);
}

static void bc_op_posr(void) {
	uint8_t reg = bc_next_u8();
	bc_set_cur_led((size_t) sRegisters[reg]);
}

static void bc_op_posendi(void) {
	float imm = bc_next_f32();
	bc_set_cur_led(STRIP_LED_COUNT - (size_t) imm);
}

static void bc_op_posendr(void) {
	uint8_t reg = bc_next_u8();
	bc_set_cur_led(STRIP_LED_COUNT - (size_t) sRegisters[reg]);
}

/* Halt instruction */

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

static void bc_update_rng(void) {
	if (sRng == 1) {
		sRng = 0;
		return;
	}

	if (sRng == 0) {
		sRng = 1;
	}
	sRng = (sRng >> 1) ^ (-(sRng & 1) & 0x80200003);
}

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

static void bc_execute(void) {
	if (sError) {
		return;
	}

	sInstrs = 0;
	sRunning = true;

	memset(sRegisters, 0, sizeof(sRegisters));
	sPc = 2;

	while (sRunning) {
		if (sPc >= gBytecodeLen) {
			sRunning = false;
			break;
		}

		uint8_t opcode = gBytecode[sPc];
		sPc++;

		if (sOps[opcode] == NULL) {
			ERROR("invalid opcode %02x", opcode);
			break;
		}

		sOps[opcode]();
		sInstrs++;

		if (sInstrs > BC_MAX_INSTRS) {
			ERROR("exceeded instruction cap");
			break;
		}

		bc_update_rng();
	}
}

static void bc_task(void *pvParameters) {
	while (true) {
		strip_suspend();
		strip_reset();

		switch (gBytecode[1]) {
			case BC_MODE_PER_LED:
				for (sCurLed = 0; sCurLed < STRIP_LED_COUNT; sCurLed++) {
					bc_execute();
				}
				break;

			case BC_MODE_PER_TICK:
				bc_execute();
				break;
		}

		strip_resume();

		if (sError) {
			sError = false;
			continue;
		}

		sTicks++;

		TickType_t delay = pdMS_TO_TICKS(sPeriodMs);
		xTaskNotifyWait(0, 0, NULL, delay < 1 ? 1 : delay);
	}
}

void bc_init(void) {
	bc_update(sInitBytecode, false);
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

bool bc_update(uint8_t *bytecode, bool checkCrc) {
	size_t len = bc_len(bytecode);

	if (checkCrc) {
		uint8_t crc = bc_crc(bytecode, len);
		if (bytecode[0] != crc) {
			return false;
		}
	}

	sTicks = 0;
	sPeriodMs = 1000;
	memset(sMemory, 0, sizeof(sMemory));

	memcpy(gBytecode, bytecode, len);
	gBytecodeLen = len;

	return true;
}

void bc_interrupt(void) {
	xTaskNotifyGive(sBytecodeTask);
}


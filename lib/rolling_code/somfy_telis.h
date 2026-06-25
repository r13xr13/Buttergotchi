#ifndef SOMFY_TELIS_H
#define SOMFY_TELIS_H

#include <stdint.h>
#include <stdbool.h>
#include "rolling_code_types.h"
#include "manchester.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Somfy Telis 56-bit Protocol Constants
// ═══════════════════════════════════════════════════════════════════════════

#define SOMFY_TELIS_BITS         56
#define SOMFY_TELIS_TE_SHORT     640
#define SOMFY_TELIS_TE_LONG      1280
#define SOMFY_TELIS_TE_DELTA     250

// ═══════════════════════════════════════════════════════════════════════════
// API Functions
// ═══════════════════════════════════════════════════════════════════════════

// Decode: XOR-decrypt a raw 56-bit frame and extract btn, cnt, serial
// into the provided BlockGeneric.
void somfy_telis_decode(BlockGeneric* gen);

// CRC check: XOR-decrypt the data, compute nibble checksum across all
// 7 bytes, and compare with the stored CRC nibble.
// Returns true if the CRC is valid.
bool somfy_telis_crc_check(uint64_t data);

// Generate: given serial, btn, and cnt, build the 7-byte plaintext frame,
// compute and insert the CRC nibble, apply XOR obfuscation, and pack
// into a 56-bit value (MSB first, in lower 56 bits of uint64_t).
uint64_t somfy_telis_generate(uint32_t serial, uint8_t btn, uint16_t cnt);

// Build timing: generate the full LevelDuration array for transmitting
// the 56-bit frame using the Somfy Telis protocol (wake-up, sync pulses,
// Manchester-encoded data, silence gap, 2 retransmissions).
// Returns the number of LevelDuration entries written to buf.
int somfy_telis_build_timing(uint64_t frame, LevelDuration* buf, int buf_max);

#ifdef __cplusplus
}
#endif

#endif // SOMFY_TELIS_H

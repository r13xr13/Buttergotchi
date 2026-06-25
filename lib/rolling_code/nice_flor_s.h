#ifndef NICE_FLOR_S_H
#define NICE_FLOR_S_H

#include <stdint.h>
#include <stdbool.h>
#include "rolling_code_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NICE_FLORS_BITS          52
#define NICE_ONE_BITS            72
#define NICE_FLORS_TE_SHORT      500
#define NICE_FLORS_TE_LONG       1000
#define NICE_FLORS_TE_DELTA      300

// Default rainbow table size (32 bytes)
#define NICE_RAINBOW_TABLE_SIZE  32

// Encrypt data using the rainbow table
// Returns encrypted 48-bit value (or 0 if no table available)
uint64_t nice_flor_s_encrypt(uint64_t data, const uint8_t* rainbow_table);

// Decrypt data using the rainbow table
// Returns decrypted 48-bit value (or 0 if no table available)
uint64_t nice_flor_s_decrypt(uint64_t data, const uint8_t* rainbow_table);

// Generate 52-bit frame from serial, btn, cnt
// If rainbow_table is NULL, uses default table
uint64_t nice_flor_s_generate(uint32_t serial, uint8_t btn, uint16_t cnt,
                              const uint8_t* rainbow_table);

// Decode 52-bit frame: extract serial, btn, cnt
void nice_flor_s_decode(BlockGeneric* gen, const uint8_t* rainbow_table);

// Build timing array for 52-bit frame (16 parcels)
// Returns number of LevelDurations written
int nice_flor_s_build_timing(uint64_t data, LevelDuration* buf, int buf_max);

// Get default rainbow table
const uint8_t* nice_flor_s_default_rainbow_table(void);

#ifdef __cplusplus
}
#endif

#endif // NICE_FLOR_S_H

#ifndef CAME_ATOMO_H
#define CAME_ATOMO_H

#include <stdint.h>
#include <stdbool.h>
#include "rolling_code_types.h"
#include "manchester.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAME_ATOMO_BITS          62
#define CAME_ATOMO_TE_SHORT      600
#define CAME_ATOMO_TE_LONG       1200
#define CAME_ATOMO_TE_DELTA      250

// Encrypt/decrypt an 8-byte packet (self-contained cipher)
void atomo_encrypt(uint8_t* buff);
void atomo_decrypt(uint8_t* buff);

// Generate 62-bit frame from serial and counter
// btn is derived: 0x1->0x0, 0x2->0x2, 0x3->0x4, 0x4->0x6, 0x5->0x0C, 0x6->0x0E
uint64_t came_atomo_generate(uint32_t serial, uint16_t cnt, uint8_t btn);

// Decode 62-bit frame: extract serial, btn, cnt, cnt_2 into BlockGeneric
void came_atomo_decode(BlockGeneric* gen);

// Build full timing array (8 parcels + final encrypted frame)
// Returns number of LevelDurations written
int came_atomo_build_timing(uint64_t frame_data, uint64_t data_2,
                            uint16_t cnt, uint8_t btn,
                            LevelDuration* buf, int buf_max);

#ifdef __cplusplus
}
#endif

#endif // CAME_ATOMO_H

#ifndef BLOCK_CODEC_H
#define BLOCK_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "rolling_code_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Decoder functions (from blocks/decoder.c)
// ═══════════════════════════════════════════════════════════════════════════

void decoder_add_bit(BlockDecoder* decoder, uint8_t bit);

void decoder_add_to_128_bit(
    BlockDecoder* decoder, uint8_t bit, uint64_t* head_64_bit);

uint8_t decoder_get_hash_data(BlockDecoder* decoder, size_t len);

// ═══════════════════════════════════════════════════════════════════════════
// Encoder functions (from blocks/encoder.c)
// ═══════════════════════════════════════════════════════════════════════════

void encoder_set_bit_array(
    bool bit_value, uint8_t data_array[], size_t set_index_bit, size_t max_size_array);

bool encoder_get_bit_array(uint8_t data_array[], size_t read_index_bit);

size_t encoder_get_upload_from_bit_array(
    uint8_t data_array[],
    size_t count_bit_data_array,
    LevelDuration* upload,
    size_t max_size_upload,
    uint32_t duration_bit,
    BlockAlignBit align_bit);

// ═══════════════════════════════════════════════════════════════════════════
// Math/CRC functions (from blocks/math.c)
// ═══════════════════════════════════════════════════════════════════════════

uint64_t reverse_key(uint64_t key, uint8_t bit_count);
uint8_t get_parity(uint64_t key, uint8_t bit_count);

uint8_t crc4(uint8_t const message[], size_t size, uint8_t polynomial, uint8_t init);
uint8_t crc7(uint8_t const message[], size_t size, uint8_t polynomial, uint8_t init);
uint8_t crc8(uint8_t const message[], size_t size, uint8_t polynomial, uint8_t init);
uint8_t crc8le(uint8_t const message[], size_t size, uint8_t polynomial, uint8_t init);
uint16_t crc16lsb(uint8_t const message[], size_t size, uint16_t polynomial, uint16_t init);
uint16_t crc16(uint8_t const message[], size_t size, uint16_t polynomial, uint16_t init);

uint8_t lfsr_digest8(uint8_t const message[], size_t size, uint8_t gen, uint8_t key);
uint8_t lfsr_digest8_reflect(uint8_t const message[], size_t size, uint8_t gen, uint8_t key);
uint16_t lfsr_digest16(uint8_t const message[], size_t size, uint16_t gen, uint16_t key);

uint8_t add_bytes(uint8_t const message[], size_t size);
uint8_t parity8(uint8_t byte);
uint8_t parity_bytes(uint8_t const message[], size_t size);
uint8_t xor_bytes(uint8_t const message[], size_t size);

#ifdef __cplusplus
}
#endif

#endif // BLOCK_CODEC_H

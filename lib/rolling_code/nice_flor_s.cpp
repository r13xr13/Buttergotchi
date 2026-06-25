#include "nice_flor_s.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// Default rainbow table (32 bytes)
// On Flipper Zero this is loaded from an SD card file.
// For GRIM, we hardcode all zeros so the code compiles standalone.
// Users can replace by loading from SD and passing the custom table.
// ═══════════════════════════════════════════════════════════════════════════

static const uint8_t default_rainbow_table[NICE_RAINBOW_TABLE_SIZE] = {0};

const uint8_t* nice_flor_s_default_rainbow_table(void) {
    return default_rainbow_table;
}

// ═══════════════════════════════════════════════════════════════════════════
// ENCRYPT (ported from Flipper Unleashed subghz/protocols/nice_flor_s.c)
//
// Processes 48 bits (6 bytes) through 2 rounds of XOR with rainbow table,
// byte swapping, and a final bit permutation.
// Returns the encrypted 48-bit value (or 0 if rainbow_table is NULL).
// ═══════════════════════════════════════════════════════════════════════════

uint64_t nice_flor_s_encrypt(uint64_t data, const uint8_t* rainbow_table) {
    if(!rainbow_table) return 0;

    uint8_t* p = (uint8_t*)&data;
    uint8_t k = 0;

    for(uint8_t y = 0; y < 2; y++) {
        // Round A: use low 5 bits of p[0] as table index
        k = rainbow_table[p[0] & 0x1f];
        for(uint8_t i = 1; i < 6; i++) p[i] ^= k;
        p[5] &= 0x0f;
        p[0] ^= k & 0xe0;

        // Round B: use bits 3-7 of p[0] as table index, add offset
        k = rainbow_table[p[0] >> 3] + 0x25;
        for(uint8_t i = 1; i < 6; i++) p[i] ^= k;
        p[5] &= 0x0f;
        p[0] ^= k & 0x7;

        // First round only: swap bytes 0 and 1
        if(y == 0) {
            k = p[0];
            p[0] = p[1];
            p[1] = k;
        }
    }

    // Final permutation (bit inversion and byte reorder)
    p[5] = ~p[5] & 0x0f;
    k = ~p[4];
    p[4] = ~p[0];
    p[0] = ~p[2];
    p[2] = k;
    k = ~p[3];
    p[3] = ~p[1];
    p[1] = k;

    return data;
}

// ═══════════════════════════════════════════════════════════════════════════
// DECRYPT (port of the Flipper decrypt function)
//
// Inverses the encrypt operation step-by-step:
//  1. Reverse final permutation
//  2. Two rounds (y=0..1) with rounds applied in reverse order
// Returns decrypted 48-bit value (or 0 if rainbow_table is NULL).
// ═══════════════════════════════════════════════════════════════════════════

uint64_t nice_flor_s_decrypt(uint64_t data, const uint8_t* rainbow_table) {
    if(!rainbow_table) return 0;

    uint8_t* p = (uint8_t*)&data;
    uint8_t k = 0;

    // Reverse final permutation
    k = ~p[4];
    p[5] = ~p[5];
    p[4] = ~p[2];
    p[2] = ~p[0];
    p[0] = k;
    k = ~p[3];
    p[3] = ~p[1];
    p[1] = k;

    for(uint8_t y = 0; y < 2; y++) {
        // Reverse Round B: uses bits 3-7 of p[0] as index
        k = rainbow_table[p[0] >> 3] + 0x25;
        for(uint8_t i = 1; i < 6; i++) p[i] ^= k;
        p[5] &= 0x0f;
        p[0] ^= k & 0x7;

        // Reverse Round A: uses low 5 bits of p[0] as index
        k = rainbow_table[p[0] & 0x1f];
        for(uint8_t i = 1; i < 6; i++) p[i] ^= k;
        p[5] &= 0x0f;
        p[0] ^= k & 0xe0;

        // First round only: swap bytes 0 and 1 back
        if(y == 0) {
            k = p[0];
            p[0] = p[1];
            p[1] = k;
        }
    }

    return data;
}

// ═══════════════════════════════════════════════════════════════════════════
// GENERATE 52-bit frame
//
// Constructs: [Btn(4) | P1(4)] [Encrypted: serial(28) | cnt(16)]
//   - Encrypts (serial << 16 | cnt) using the rainbow table
//   - Places button byte at bits 44..51:
//       byte = btn << 4 | (0xF ^ btn ^ 0x3)
//   - Lower 44 bits carry the encrypted payload
// If rainbow_table is NULL, the default table is used.
// ═══════════════════════════════════════════════════════════════════════════

uint64_t nice_flor_s_generate(
    uint32_t serial, uint8_t btn, uint16_t cnt,
    const uint8_t* rainbow_table)
{
    if(!rainbow_table) {
        rainbow_table = default_rainbow_table;
    }

    // Pack serial (28 bits) and counter (16 bits) into 44-bit plaintext
    uint64_t decrypt = ((uint64_t)serial << 16) | cnt;

    // Encrypt the 44-bit value (padded to 48 bits internally)
    uint64_t enc_part = nice_flor_s_encrypt(decrypt, rainbow_table);

    // Build button byte: btn in upper nibble, (0xF ^ btn ^ 0x3) in lower
    uint8_t byte = (uint8_t)(btn << 4) | (uint8_t)(0xF ^ btn ^ 0x3);

    // Assemble 52-bit frame:
    //   bits 51..44 = button byte
    //   bits 43..0  = encrypted payload (44 bits)
    // enc_part is 48 bits but bits 44..47 are always 0 (p[5] is masked to 0x0f
    // and the final permutation & 0x0f keeps only the lower nibble).
    return ((uint64_t)byte << 44) | (enc_part & 0xFFFFFFFFFFFFULL);
}

// ═══════════════════════════════════════════════════════════════════════════
// DECODE 52-bit frame
//
// Decrypts gen->data using the rainbow table and extracts:
//   - gen->cnt    = lower 16 bits
//   - gen->serial = bits 16..43 (28 bits)
//   - gen->btn    = bits 48..51
// If rainbow_table is NULL, the default table is used.
// ═══════════════════════════════════════════════════════════════════════════

void nice_flor_s_decode(BlockGeneric* gen, const uint8_t* rainbow_table) {
    if(!gen) return;

    if(!rainbow_table) {
        rainbow_table = default_rainbow_table;
    }

    uint64_t decrypt = nice_flor_s_decrypt(gen->data, rainbow_table);

    gen->cnt = (uint32_t)(decrypt & 0xFFFF);
    gen->serial = (uint32_t)((decrypt >> 16) & 0xFFFFFFF);
    gen->btn = (uint8_t)((decrypt >> 48) & 0xF);
}

// ═══════════════════════════════════════════════════════════════════════════
// BUILD TIMING — encode 16 parcels of a 52-bit Nice Flor-S frame
//
// The protocol sends the same encrypted payload 16 times, each with a
// different P1 nibble (0..15) in the button byte for redundancy.
//
// data — 52-bit frame as produced by nice_flor_s_generate()
//        (button byte at bits 44..51, encrypted payload in bits 43..0)
//
// For each of the 16 parcels:
//   1. Build button byte:  btn << 4 | (0xF ^ btn ^ parcel_index)
//   2. Build 52-bit data with the same encrypted payload
//   3. Header:  low 18500 us (te_short * 37)
//   4. Start:   high 1500 us, low 1500 us
//   5. 52 data bits NRZ-encoded MSB first:
//        bit = 1  →  high 1000 us  +  low 500 us
//        bit = 0  →  high 500 us   +  low 1000 us
//   6. Stop:    high 1500 us
//
// Returns number of LevelDuration entries written.
// ═══════════════════════════════════════════════════════════════════════════

int nice_flor_s_build_timing(uint64_t data, LevelDuration* buf, int buf_max) {
    if(!buf || buf_max <= 0) return 0;

    // Extract button number from bits 48..51 of the 52-bit frame
    uint8_t btn = (uint8_t)((data >> 48) & 0x0F);

    // Extract clean encrypted payload (bits 0..43, 44 bits).
    // Bits 44..47 carry only the P1 nibble from the original frame,
    // not part of the encrypted data (encrypt masks p[5] to 0x0f).
    uint64_t enc_part = data & 0xFFFFFFFFFFFULL;

    int index = 0;

    for(uint8_t parcel = 0; parcel < 16; parcel++) {
        // Safety check: each parcel needs 1 + 2 + 52*2 + 1 = 108 entries
        if(index + 108 > buf_max) break;

        // Build button byte with parcel-specific P1
        uint8_t byte = (uint8_t)(btn << 4) | (uint8_t)(0xF ^ btn ^ parcel);
        uint64_t parcel_data = ((uint64_t)byte << 44) | enc_part;

        // 1. Header: low 18500 us (silence / space)
        buf[index++] = level_duration_make(false, NICE_FLORS_TE_SHORT * 37);

        // 2. Start bit: high 1500 us, low 1500 us
        buf[index++] = level_duration_make(true, NICE_FLORS_TE_SHORT * 3);
        buf[index++] = level_duration_make(false, NICE_FLORS_TE_SHORT * 3);

        // 3. Encode 52 data bits NRZ, MSB first
        //    NRZ: 1 = long high + short low,  0 = short high + long low
        for(int bit = 51; bit >= 0; bit--) {
            if(bit_read(parcel_data, bit)) {
                buf[index++] = level_duration_make(true, NICE_FLORS_TE_LONG);
                buf[index++] = level_duration_make(false, NICE_FLORS_TE_SHORT);
            } else {
                buf[index++] = level_duration_make(true, NICE_FLORS_TE_SHORT);
                buf[index++] = level_duration_make(false, NICE_FLORS_TE_LONG);
            }
        }

        // 4. Stop bit: high 1500 us (no trailing low)
        buf[index++] = level_duration_make(true, NICE_FLORS_TE_SHORT * 3);
    }

    return index;
}

#include "came_atomo.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// Button code mapping tables
// ═══════════════════════════════════════════════════════════════════════════

// Encode: standard btn (1..6) -> CAME Atomo nibble
static const uint8_t atomo_btn_enc_map[] =
    {0, 0x0, 0x2, 0x4, 0x6, 0x0C, 0x0E};

// Decode: CAME Atomo nibble -> standard btn (1..6)
static const uint8_t atomo_btn_dec_map[] =
    {1, 0, 2, 0, 3, 0, 4, 0, 0, 0, 0, 0, 5, 0, 6, 0};

// ═══════════════════════════════════════════════════════════════════════════
// Parcel counter values for the 8 transmission parcels
// ═══════════════════════════════════════════════════════════════════════════

static const uint8_t atomo_parcel_cnt[] = {10, 30, 125, 126, 127, 0, 1, 3};

// ═══════════════════════════════════════════════════════════════════════════
// ManchesterEncoderResult -> LevelDuration converter
// ═══════════════════════════════════════════════════════════════════════════

static LevelDuration atomo_manchester_to_level(ManchesterEncoderResult result) {
    switch(result) {
    case ManchesterEncoderResultShortLow:
        return level_duration_make(false, CAME_ATOMO_TE_SHORT);
    case ManchesterEncoderResultLongLow:
        return level_duration_make(false, CAME_ATOMO_TE_LONG);
    case ManchesterEncoderResultLongHigh:
        return level_duration_make(true, CAME_ATOMO_TE_LONG);
    case ManchesterEncoderResultShortHigh:
        return level_duration_make(true, CAME_ATOMO_TE_SHORT);
    }
    return level_duration_make(false, CAME_ATOMO_TE_SHORT);
}

// ═══════════════════════════════════════════════════════════════════════════
// Build an 8-byte encrypted packet from fields + cnt_2
// ═══════════════════════════════════════════════════════════════════════════

static void atomo_build_packet(uint8_t* pack, uint32_t serial,
                               uint16_t cnt, uint8_t btn, uint8_t cnt_2) {
    uint8_t btn_enc = (btn < 7) ? atomo_btn_enc_map[btn] : 0;

    pack[0] = cnt_2;
    pack[1] = (cnt >> 8) & 0xFF;
    pack[2] = cnt & 0xFF;
    pack[3] = (serial >> 24) & 0xFF;
    pack[4] = (serial >> 16) & 0xFF;
    pack[5] = (serial >> 8) & 0xFF;
    pack[6] = serial & 0xFF;
    pack[7] = (uint8_t)(btn_enc << 4);

    atomo_encrypt(pack);
}

// ═══════════════════════════════════════════════════════════════════════════
// Convert an 8-byte encrypted packet -> 60-bit frame (inverted, shifted)
// ═══════════════════════════════════════════════════════════════════════════

static uint64_t atomo_pack_to_frame(const uint8_t* pack) {
    uint32_t hi = ((uint32_t)pack[0] << 24) |
                  ((uint32_t)pack[1] << 16) |
                  ((uint32_t)pack[2] <<  8) |
                   pack[3];
    uint32_t lo = ((uint32_t)pack[4] << 24) |
                  ((uint32_t)pack[5] << 16) |
                  ((uint32_t)pack[6] <<  8) |
                   pack[7];

    uint64_t data = ((uint64_t)hi << 32) | lo;
    data ^= 0xFFFFFFFFFFFFFFFFULL;          // invert all bits
    data >>= 4;                              // shift right 4
    data &= 0x0FFFFFFFFFFFFFFFULL;           // mask to 60 bits
    return data;
}

// ═══════════════════════════════════════════════════════════════════════════
// Convert a 60-bit frame back to 8-byte encrypted packet (reverse of above)
// ═══════════════════════════════════════════════════════════════════════════

static void atomo_frame_to_pack(uint64_t frame, uint8_t* pack) {
    uint64_t temp = frame & 0x0FFFFFFFFFFFFFFFULL;
    temp <<= 4;
    temp = ~temp;
    temp &= 0xFFFFFFFFFFFFFFF0ULL; // bottom nibble was always 0 in original

    for(int i = 0; i < 8; i++) {
        pack[i] = (temp >> (56 - i * 8)) & 0xFF;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Manchester-encode 60 bits (inverted) into the buffer
// Returns the number of LevelDurations written
// ═══════════════════════════════════════════════════════════════════════════

static size_t atomo_encode_manchester(uint64_t data_60,
                                      LevelDuration* buf, size_t buf_max) {
    ManchesterEncoderState state;
    manchester_encoder_reset(&state);
    ManchesterEncoderResult result;
    size_t idx = 0;

    // Encode bits 59 down to 0 (all 60 frame bits)
    for(int i = 59; i >= 0; i--) {
        if(idx >= buf_max) break;

        bool bit = !bit_read(data_60, i);

        // First call: if advance=false, the intermediate result is valid
        if(!manchester_encoder_advance(&state, bit, &result)) {
            if(idx >= buf_max) break;
            buf[idx++] = atomo_manchester_to_level(result);
            if(idx >= buf_max) break;
            manchester_encoder_advance(&state, bit, &result);
        }
        buf[idx++] = atomo_manchester_to_level(result);
    }

    // Finish: produce final short transition if ending on HIGH
    if(idx < buf_max) {
        result = manchester_encoder_finish(&state);
        LevelDuration ld = atomo_manchester_to_level(result);
        if(level_duration_get_level(ld) && idx < buf_max) {
            buf[idx++] = ld;
        }
    }

    return idx;
}

// ═══════════════════════════════════════════════════════════════════════════
// atomo_encrypt — self-contained cipher for CAME Atomo 62-bit protocol
// ═══════════════════════════════════════════════════════════════════════════

void atomo_encrypt(uint8_t* buff) {
    uint8_t tmpB = (~buff[0] + 1) & 0x7F;
    uint8_t bitCnt = 8;
    while(bitCnt < 59) {
        if((tmpB & 0x18) && (((tmpB / 8) & 3) != 3)) {
            tmpB = ((tmpB << 1) & 0xFF) | 1;
        } else {
            tmpB = (tmpB << 1) & 0xFF;
        }
        if(tmpB & 0x80) {
            buff[bitCnt / 8] ^= (0x80 >> (bitCnt & 7));
        }
        bitCnt++;
    }
    buff[0] = (buff[0] ^ 5) & 0x7F;
}

// ═══════════════════════════════════════════════════════════════════════════
// atomo_decrypt — inverse of atomo_encrypt
// ═══════════════════════════════════════════════════════════════════════════

void atomo_decrypt(uint8_t* buff) {
    buff[0] = (buff[0] ^ 5) & 0x7F;
    uint8_t tmpB = (-buff[0]) & 0x7F;
    uint8_t bitCnt = 8;
    while(bitCnt < 59) {
        if((tmpB & 0x18) && (((tmpB / 8) & 3) != 3)) {
            tmpB = ((tmpB << 1) & 0xFF) | 1;
        } else {
            tmpB = (tmpB << 1) & 0xFF;
        }
        if(tmpB & 0x80) {
            buff[bitCnt / 8] ^= (0x80 >> (bitCnt & 7));
        }
        bitCnt++;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// came_atomo_generate — create a 62-bit (60-bit effective) frame
// ═══════════════════════════════════════════════════════════════════════════

uint64_t came_atomo_generate(uint32_t serial, uint16_t cnt, uint8_t btn) {
    uint8_t pack[8];
    // cnt_2 = 0x7E (initial button hold cycle counter)
    atomo_build_packet(pack, serial, cnt, btn, 0x7E);
    return atomo_pack_to_frame(pack);
}

// ═══════════════════════════════════════════════════════════════════════════
// came_atomo_decode — extract fields from a 62-bit frame into BlockGeneric
// ═══════════════════════════════════════════════════════════════════════════

void came_atomo_decode(BlockGeneric* gen) {
    if(!gen) return;

    uint8_t pack[8];
    atomo_frame_to_pack(gen->data, pack);
    atomo_decrypt(pack);

    gen->cnt_2 = pack[0];
    gen->cnt   = ((uint16_t)pack[1] << 8) | pack[2];
    gen->serial = ((uint32_t)pack[3] << 24) |
                  ((uint32_t)pack[4] << 16) |
                  ((uint32_t)pack[5] <<  8) |
                   pack[6];

    uint8_t btn_dec = pack[7] >> 4;
    gen->btn = (btn_dec < 16) ? atomo_btn_dec_map[btn_dec] : 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// came_atomo_build_timing — build the full 8-parcel transmission
//
// frame_data: pre-computed 60-bit frame from came_atomo_generate
//             (used to extract serial)
// data_2:     unused (reserved for extended use)
// cnt:        rolling counter
// btn:        button code (1..6)
// buf:        output LevelDuration buffer
// buf_max:    maximum entries in buf
// Returns:    number of LevelDurations written, or -1 on error
// ═══════════════════════════════════════════════════════════════════════════

int came_atomo_build_timing(uint64_t frame_data, uint64_t data_2,
                            uint16_t cnt, uint8_t btn,
                            LevelDuration* buf, int buf_max) {
    (void)data_2;

    if(!buf || buf_max < 1) return -1;

    // ── Extract serial from the pre-computed frame ──
    uint8_t pack[8];
    atomo_frame_to_pack(frame_data, pack);
    atomo_decrypt(pack);

    uint32_t serial = ((uint32_t)pack[3] << 24) |
                      ((uint32_t)pack[4] << 16) |
                      ((uint32_t)pack[5] <<  8) |
                       pack[6];

    // ── Build 8 parcels ──
    size_t idx = 0;

    for(int p = 0; p < 8; p++) {
        // Check if we have enough buffer space (safe upper-bound estimate)
        // Worst case per parcel: 2 (header) + ~120 (Manchester) + 1 (pause)
        if(idx + 130 > (size_t)buf_max) break;

        // Build encrypted packet with this parcel's counter value
        uint8_t parcel_pack[8];
        atomo_build_packet(parcel_pack, serial, cnt, btn, atomo_parcel_cnt[p]);

        // Convert to 60-bit frame data
        uint64_t parcel_data = atomo_pack_to_frame(parcel_pack);

        // ── Header: high 18000μs, low 72000μs ──
        buf[idx++] = level_duration_make(true, 18000);
        buf[idx++] = level_duration_make(false, 72000);

        // ── Manchester encode 60 bits ──
        size_t remaining = (size_t)buf_max - idx;
        size_t written = atomo_encode_manchester(parcel_data, buf + idx, remaining);
        idx += written;

        if(idx >= (size_t)buf_max) break;

        // ── Pause: low 68000μs (te_delta * 272) ──
        buf[idx++] = level_duration_make(false, 68000);
    }

    return (int)idx;
}

#include "somfy_telis.h"

// ═══════════════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════════════

// Compute the nibble checksum (XOR of (byte ^ byte>>4) for each of 7 bytes,
// folded to 4 bits).  Works on a 7-byte plaintext frame.
static uint8_t somfy_telis_crc_nibble(const uint8_t frame[7]) {
    uint8_t crc = 0;
    for(int i = 0; i < 7; i++) {
        crc ^= frame[i] ^ (frame[i] >> 4);
    }
    return crc & 0x0F;
}

// Extract byte i (0..6, MSB first) from a 56-bit value stored in a uint64_t.
static inline uint8_t byte_from_frame(uint64_t data, int i) {
    return (uint8_t)((data >> (48 - i * 8)) & 0xFF);
}

// ═══════════════════════════════════════════════════════════════════════════
// DECODE
// ═══════════════════════════════════════════════════════════════════════════

void somfy_telis_decode(BlockGeneric* gen) {
    uint64_t data = gen->data;
    // XOR decrypt: reverses the cumulative-XOR obfuscation
    data ^= (data >> 8);

    // Frame format (decrypted): A7 | Btn(4) + CRC(4) | cnt(16) | serial(24)
    gen->btn    = (data >> 44) & 0x0F;
    gen->cnt    = (data >> 24) & 0xFFFF;
    gen->serial =  data        & 0xFFFFFF;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRC CHECK
// ═══════════════════════════════════════════════════════════════════════════

bool somfy_telis_crc_check(uint64_t data) {
    // XOR decrypt first
    data ^= (data >> 8);

    // Compute nibble checksum across all 7 bytes
    uint8_t crc = 0;
    for(int i = 0; i < 7; i++) {
        uint8_t byte = byte_from_frame(data, i);
        crc ^= byte ^ (byte >> 4);
    }
    crc &= 0x0F;

    // Compare with the stored CRC nibble (lower nibble of byte[1])
    uint8_t stored_crc = (data >> 40) & 0x0F;
    return crc == stored_crc;
}

// ═══════════════════════════════════════════════════════════════════════════
// GENERATE
// ═══════════════════════════════════════════════════════════════════════════

uint64_t somfy_telis_generate(uint32_t serial, uint8_t btn, uint16_t cnt) {
    // ── Build 7-byte plaintext frame ──
    uint8_t frame[7];
    frame[0] = 0xA7;
    frame[1] = (btn << 4);           // CRC nibble set to 0 temporarily
    frame[2] = (uint8_t)(cnt >> 8);
    frame[3] = (uint8_t)(cnt & 0xFF);
    frame[4] = (uint8_t)(serial >> 16);
    frame[5] = (uint8_t)(serial >> 8);
    frame[6] = (uint8_t)(serial & 0xFF);

    // ── Compute and insert CRC nibble ──
    uint8_t crc = somfy_telis_crc_nibble(frame);
    frame[1] = (btn << 4) | crc;

    // ── XOR obfuscation (cumulative XOR left-to-right) ──
    for(int i = 1; i < 7; i++) {
        frame[i] ^= frame[i - 1];
    }

    // ── Pack into uint64_t MSB first ──
    uint64_t result = 0;
    for(int i = 0; i < 7; i++) {
        result = (result << 8) | frame[i];
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// BUILD TIMING
// ═══════════════════════════════════════════════════════════════════════════

// Helpers to convert a ManchesterEncoderResult to a LevelDuration using
// the Somfy Telis timing.
static inline LevelDuration manchester_result_to_ld(ManchesterEncoderResult r) {
    static const LevelDuration table[4] = {
        level_duration_make(true,  SOMFY_TELIS_TE_SHORT),  // ShortLow  -> high 640
        level_duration_make(false, SOMFY_TELIS_TE_LONG),   // LongLow   -> low  1280
        level_duration_make(true,  SOMFY_TELIS_TE_LONG),   // LongHigh  -> high 1280
        level_duration_make(false, SOMFY_TELIS_TE_SHORT),  // ShortHigh -> low  640
    };
    return table[r];
}

// Append one Manchester-encoded 56-bit frame to the buffer.
// Returns the new buffer index (may exceed buf_max if there wasn't room).
static int append_manchester_data(uint64_t frame, LevelDuration* buf, int idx, int buf_max) {
    ManchesterEncoderState enc;
    manchester_encoder_reset(&enc);

    // Encode bits MSB first (bit 55 down to bit 0)
    for(int b = SOMFY_TELIS_BITS - 1; b >= 0; b--) {
        bool bit = bit_read(frame, b);
        bool done;
        do {
            if(idx >= buf_max) return idx;
            ManchesterEncoderResult result;
            done = manchester_encoder_advance(&enc, bit, &result);
            if(done) {
                buf[idx++] = manchester_result_to_ld(result);
            }
        } while(!done);
    }

    // Finish the encoder — produce the final half-bit
    if(idx < buf_max) {
        ManchesterEncoderResult result = manchester_encoder_finish(&enc);
        buf[idx++] = manchester_result_to_ld(result);
    }
    return idx;
}

static inline int append_ld(LevelDuration* buf, int idx, bool level, uint32_t dur) {
    buf[idx] = level_duration_make(level, dur);
    return idx + 1;
}

// ── Wake-up / sync pulse helpers ──

static int append_wakeup(LevelDuration* buf, int idx, int buf_max) {
    if(idx + 2 > buf_max) return idx;
    idx = append_ld(buf, idx, true,  9415);
    idx = append_ld(buf, idx, false, 89565);
    return idx;
}

static int append_hw_sync(LevelDuration* buf, int idx, int count, int buf_max) {
    for(int i = 0; i < count; i++) {
        if(idx + 2 > buf_max) return idx;
        idx = append_ld(buf, idx, true,  2560);
        idx = append_ld(buf, idx, false, 2560);
    }
    return idx;
}

static int append_sw_sync(LevelDuration* buf, int idx, int buf_max) {
    if(idx + 2 > buf_max) return idx;
    idx = append_ld(buf, idx, true,  4550);
    idx = append_ld(buf, idx, false, 640);
    return idx;
}

static int append_silence(LevelDuration* buf, int idx, int buf_max) {
    if(idx >= buf_max) return idx;
    idx = append_ld(buf, idx, false, 30415);
    return idx;
}

// ── Main build_timing ──

int somfy_telis_build_timing(uint64_t frame, LevelDuration* buf, int buf_max) {
    int idx = 0;

    // Transmission 1: wake-up, 2× hardware sync, software sync, data, silence
    idx = append_wakeup(buf, idx, buf_max);
    idx = append_hw_sync(buf, idx, 2, buf_max);
    idx = append_sw_sync(buf, idx, buf_max);
    idx = append_manchester_data(frame, buf, idx, buf_max);
    idx = append_silence(buf, idx, buf_max);

    // Retransmissions 2 & 3: 7× hardware sync, software sync, data, silence
    for(int tx = 1; tx < 3; tx++) {
        idx = append_hw_sync(buf, idx, 7, buf_max);
        idx = append_sw_sync(buf, idx, buf_max);
        idx = append_manchester_data(frame, buf, idx, buf_max);
        idx = append_silence(buf, idx, buf_max);
    }

    return idx;
}

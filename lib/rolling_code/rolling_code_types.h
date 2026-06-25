#ifndef ROLLING_CODE_TYPES_H
#define ROLLING_CODE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════
// LevelDuration — replaces Flipper's lib/toolbox/level_duration.h
// Used by protocol encoders to describe OOK signal timing
// ═══════════════════════════════════════════════════════════════════════════

#define LEVEL_DURATION_RESET      0U
#define LEVEL_DURATION_LEVEL_LOW  1U
#define LEVEL_DURATION_LEVEL_HIGH 2U
#define LEVEL_DURATION_WAIT       3U
#define LEVEL_DURATION_RESERVED   0x800000U

typedef struct {
    uint32_t duration : 30;
    uint8_t level     : 2;
} LevelDuration;

static inline LevelDuration level_duration_make(bool level, uint32_t duration) {
    LevelDuration ld;
    ld.level = level ? LEVEL_DURATION_LEVEL_HIGH : LEVEL_DURATION_LEVEL_LOW;
    ld.duration = duration;
    return ld;
}

static inline LevelDuration level_duration_reset(void) {
    LevelDuration ld;
    ld.level = LEVEL_DURATION_RESET;
    return ld;
}

static inline LevelDuration level_duration_wait(void) {
    LevelDuration ld;
    ld.level = LEVEL_DURATION_WAIT;
    return ld;
}

static inline bool level_duration_is_reset(LevelDuration ld) {
    return ld.level == LEVEL_DURATION_RESET;
}

static inline bool level_duration_is_wait(LevelDuration ld) {
    return ld.level == LEVEL_DURATION_WAIT;
}

static inline bool level_duration_get_level(LevelDuration ld) {
    return ld.level == LEVEL_DURATION_LEVEL_HIGH;
}

static inline uint32_t level_duration_get_duration(LevelDuration ld) {
    return ld.duration;
}

// ═══════════════════════════════════════════════════════════════════════════
// Protocol Status — replaces SubGhzProtocolStatus from types.h
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    RollingCodeStatusOk = 0,
    RollingCodeStatusError = -1,
    RollingCodeStatusErrorParserHeader = -2,
    RollingCodeStatusErrorParserFrequency = -3,
    RollingCodeStatusErrorParserPreset = -4,
    RollingCodeStatusErrorParserCustomPreset = -5,
    RollingCodeStatusErrorParserProtocolName = -6,
    RollingCodeStatusErrorParserBitCount = -7,
    RollingCodeStatusErrorParserKey = -8,
    RollingCodeStatusErrorParserTe = -9,
    RollingCodeStatusErrorParserOthers = -10,
    RollingCodeStatusErrorValueBitCount = -11,
    RollingCodeStatusErrorEncoderGetUpload = -12,
    RollingCodeStatusErrorProtocolNotFound = -13,
} RollingCodeStatus;

// ═══════════════════════════════════════════════════════════════════════════
// Block Generic — replaces SubGhzBlockGeneric from blocks/generic.h
// Stores decoded protocol data (serial, counter, button, etc.)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    const char* protocol_name;
    uint64_t data;         // The full decoded data (reversed key)
    uint64_t data_2;       // Second data word (for 72+ bit protocols)
    uint32_t serial;       // Serial number
    uint16_t data_count_bit; // Number of bits
    uint8_t btn;           // Button code
    uint32_t cnt;          // Counter value
    uint8_t cnt_2;         // Second counter (for some protocols)
    uint32_t seed;         // Seed value (for secure learning / BFT)
} BlockGeneric;

// ═══════════════════════════════════════════════════════════════════════════
// Block Decoder — replaces SubGhzBlockDecoder from blocks/decoder.h
// State machine for bit-by-bit pulse decoding
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    uint32_t parser_step;
    uint32_t te_last;
    uint64_t decode_data;
    uint8_t decode_count_bit;
} BlockDecoder;

// ═══════════════════════════════════════════════════════════════════════════
// Block Const — replaces SubGhzBlockConst from blocks/const.h
// Protocol timing constants (te_short, te_long, te_delta)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    uint16_t te_long;
    uint16_t te_short;
    uint16_t te_delta;
    uint8_t min_count_bit_for_found;
} BlockConst;

// ═══════════════════════════════════════════════════════════════════════════
// Block Encoder — replaces SubGhzProtocolBlockEncoder from blocks/encoder.h
// State machine for generating LevelDuration upload arrays
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    bool is_running;
    size_t repeat;
    size_t front;
    size_t size_upload;
    LevelDuration* upload;
} BlockEncoder;

typedef enum {
    BlockAlignBitLeft,
    BlockAlignBitRight,
} BlockAlignBit;

// ═══════════════════════════════════════════════════════════════════════════
// Manufacturer Key Entry — replaces SubGhzKey from subghz_keystore.h
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    const char* name;
    uint64_t key;
    uint16_t type;  // KEELOQ_LEARNING_* type
} ManufacturerKey;

// ═══════════════════════════════════════════════════════════════════════════
// KeeLoq Learning Types
// ═══════════════════════════════════════════════════════════════════════════

#define KEELOQ_NLF                  0x3A5C742E

#define KEELOQ_LEARNING_UNKNOWN             0u
#define KEELOQ_LEARNING_SIMPLE              1u
#define KEELOQ_LEARNING_NORMAL              2u
#define KEELOQ_LEARNING_SECURE              3u
#define KEELOQ_LEARNING_MAGIC_XOR_TYPE_1    4u
#define KEELOQ_LEARNING_FAAC                5u
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_1 6u
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_2 7u
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_3 8u
#define KEELOQ_LEARNING_SIMPLE_KINGGATES    10u
#define KEELOQ_LEARNING_NORMAL_JAROLIFT     11u
#define KEELOQ_LEARNING_ERREKA              12u
#define KEELOQ_LEARNING_PUJOL               13u
#define KEELOQ_LEARNING_AERF                14u
#define KEELOQ_LEARNING_SIMPLE_JCM          15u

// ═══════════════════════════════════════════════════════════════════════════
// Protocol IDs for GRIM rolling code attack system
// ═══════════════════════════════════════════════════════════════════════════

#define PROTOCOL_ID_KEELOQ       0
#define PROTOCOL_ID_SOMFY_TELIS  1
#define PROTOCOL_ID_NICE_FLORS   2
#define PROTOCOL_ID_CAME_ATOMO   3
#define PROTOCOL_ID_COUNT        4

// ═══════════════════════════════════════════════════════════════════════════
// CRC & Math Macros
// ═══════════════════════════════════════════════════════════════════════════

#define bit_read(value, bit)          (((value) >> (bit)) & 0x01)
#define bit_set(value, bit)           ((value) |= (1ULL << (bit)))
#define bit_clear(value, bit)         ((value) &= ~(1ULL << (bit)))
#define bit_write(value, bit, bv)     ((bv) ? bit_set(value, bit) : bit_clear(value, bit))
#define DURATION_DIFF(x, y)           (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))

#ifdef __cplusplus
}
#endif

#endif // ROLLING_CODE_TYPES_H
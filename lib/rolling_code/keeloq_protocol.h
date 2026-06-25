#ifndef KEELOQ_PROTOCOL_H
#define KEELOQ_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "rolling_code_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KEELOQ_BITS        64
#define KEELOQ_TE_SHORT    400
#define KEELOQ_TE_LONG     800
#define KEELOQ_TE_DELTA    180

// Decoder state machine
typedef enum {
    KeeloqStepReset = 0,
    KeeloqStepCheckPreamble,
    KeeloqStepSaveDuration,
    KeeloqStepCheckDuration,
} KeeloqDecoderStep;

// KeeLoq decoder instance
typedef struct {
    BlockDecoder decoder;
    BlockGeneric generic;
    uint16_t header_count;
    KeeloqDecoderStep parser_step;
} KeeloqDecoder;

// KeeLoq encoder instance
typedef struct {
    BlockEncoder encoder;
    BlockGeneric generic;
    const ManufacturerKey* mf_key;
    uint8_t btn;
} KeeloqEncoder;

// Decoder API
void keeloq_decoder_reset(KeeloqDecoder* dec);
void keeloq_decoder_feed(KeeloqDecoder* dec, bool level, uint32_t duration);

// Encoder API
void keeloq_encoder_init(KeeloqEncoder* enc, LevelDuration* upload_buf, size_t buf_size);
bool keeloq_encoder_encode(KeeloqEncoder* enc, uint32_t serial, uint8_t btn, uint16_t cnt, const ManufacturerKey* mf_key);
LevelDuration keeloq_encoder_yield(KeeloqEncoder* enc);

// Analysis: given a received 64-bit frame, try to identify manufacturer and extract serial/btn/cnt
// Returns true if manufacturer identified, false if unknown
// When true, out fields are populated
bool keeloq_analyze(uint64_t frame, BlockGeneric* out, const ManufacturerKey* keys, size_t key_count);

// Generate the 64-bit hopping code for transmission given serial/btn/cnt and manufacturer key
bool keeloq_generate(uint32_t serial, uint8_t btn, uint16_t cnt, const ManufacturerKey* mf_key, uint64_t* out_frame);

// Get a human-readable string for the decoded frame
// buf must be at least 128 bytes
void keeloq_get_string(const BlockGeneric* generic, const char* mf_name, char* buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif // KEELOQ_PROTOCOL_H

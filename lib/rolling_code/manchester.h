#ifndef MANCHESTER_H
#define MANCHESTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Manchester Decoder — ported from lib/toolbox/manchester_decoder.c/h
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    ManchesterEventShortLow  = 0,
    ManchesterEventShortHigh = 2,
    ManchesterEventLongLow   = 4,
    ManchesterEventLongHigh  = 6,
    ManchesterEventReset     = 8
} ManchesterEvent;

typedef enum {
    ManchesterStateStart1 = 0,
    ManchesterStateMid1   = 1,
    ManchesterStateMid0   = 2,
    ManchesterStateStart0 = 3
} ManchesterState;

static inline bool manchester_advance(
    ManchesterState state,
    ManchesterEvent event,
    ManchesterState* next_state,
    bool* data)
{
    static const uint8_t transitions[] = {0b00000001, 0b10010001, 0b10011011, 0b11111011};
    static const ManchesterState reset_state = ManchesterStateMid1;

    bool result = false;
    ManchesterState new_state;

    if(event == ManchesterEventReset) {
        new_state = reset_state;
    } else {
        new_state = (ManchesterState)((transitions[state] >> event) & 0x3);
        if(new_state == state) {
            new_state = reset_state;
        } else {
            if(new_state == ManchesterStateMid0) {
                if(data) *data = false;
                result = true;
            } else if(new_state == ManchesterStateMid1) {
                if(data) *data = true;
                result = true;
            }
        }
    }

    *next_state = new_state;
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// Manchester Encoder — ported from lib/toolbox/manchester_encoder.c/h
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    bool prev_bit;
    uint8_t step;
} ManchesterEncoderState;

typedef enum {
    ManchesterEncoderResultShortLow  = 0b00,
    ManchesterEncoderResultLongLow   = 0b01,
    ManchesterEncoderResultLongHigh  = 0b10,
    ManchesterEncoderResultShortHigh = 0b11,
} ManchesterEncoderResult;

static inline void manchester_encoder_reset(ManchesterEncoderState* state) {
    state->step = 0;
}

static inline bool manchester_encoder_advance(
    ManchesterEncoderState* state,
    const bool curr_bit,
    ManchesterEncoderResult* result)
{
    bool advance = false;
    switch(state->step) {
    case 0:
        state->prev_bit = curr_bit;
        if(state->prev_bit) {
            *result = ManchesterEncoderResultShortLow;
        } else {
            *result = ManchesterEncoderResultShortHigh;
        }
        state->step = 1;
        advance = true;
        break;
    case 1:
        *result = (ManchesterEncoderResult)((state->prev_bit << 1) + curr_bit);
        if(curr_bit == state->prev_bit) {
            state->step = 2;
        } else {
            state->prev_bit = curr_bit;
            advance = true;
        }
        break;
    case 2:
        if(curr_bit) {
            *result = ManchesterEncoderResultShortLow;
        } else {
            *result = ManchesterEncoderResultShortHigh;
        }
        state->prev_bit = curr_bit;
        state->step = 1;
        advance = true;
        break;
    }
    return advance;
}

static inline ManchesterEncoderResult manchester_encoder_finish(ManchesterEncoderState* state) {
    state->step = 0;
    return (ManchesterEncoderResult)((state->prev_bit << 1) + state->prev_bit);
}

#ifdef __cplusplus
}
#endif

#endif /* MANCHESTER_H */

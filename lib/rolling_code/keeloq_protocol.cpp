#include "keeloq_protocol.h"
#include "keeloq_cipher.h"
#include "block_codec.h"
#include <string.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// Protocol timing constants
// ═══════════════════════════════════════════════════════════════════════════

static const BlockConst keeloq_const = {
    .te_long = KEELOQ_TE_LONG,
    .te_short = KEELOQ_TE_SHORT,
    .te_delta = KEELOQ_TE_DELTA,
    .min_count_bit_for_found = KEELOQ_BITS,
};

// ═══════════════════════════════════════════════════════════════════════════
// DECODER
// ═══════════════════════════════════════════════════════════════════════════

void keeloq_decoder_reset(KeeloqDecoder* dec) {
    memset(&dec->decoder, 0, sizeof(BlockDecoder));
    dec->decoder.parser_step = KeeloqStepReset;
    dec->header_count = 0;
    memset(&dec->generic, 0, sizeof(BlockGeneric));
    dec->generic.protocol_name = "KeeLoq";
    dec->generic.data_count_bit = KEELOQ_BITS;
}

void keeloq_decoder_feed(KeeloqDecoder* dec, bool level, uint32_t duration) {
    switch(dec->decoder.parser_step) {
    case KeeloqStepReset:
        if(level && DURATION_DIFF(duration, keeloq_const.te_short) < keeloq_const.te_delta) {
            dec->decoder.parser_step = KeeloqStepCheckPreamble;
            dec->header_count++;
        }
        break;

    case KeeloqStepCheckPreamble:
        if(!level && DURATION_DIFF(duration, keeloq_const.te_short) < keeloq_const.te_delta) {
            dec->decoder.parser_step = KeeloqStepReset;
            break;
        }
        if(dec->header_count > 2 &&
           DURATION_DIFF(duration, keeloq_const.te_short * 10) < keeloq_const.te_delta * 10) {
            dec->decoder.parser_step = KeeloqStepSaveDuration;
            dec->decoder.decode_data = 0;
            dec->decoder.decode_count_bit = 0;
        } else {
            dec->decoder.parser_step = KeeloqStepReset;
            dec->header_count = 0;
        }
        break;

    case KeeloqStepSaveDuration:
        if(level) {
            dec->decoder.te_last = duration;
            dec->decoder.parser_step = KeeloqStepCheckDuration;
        }
        break;

    case KeeloqStepCheckDuration:
        if(!level) {
            if(duration >= ((uint32_t)keeloq_const.te_short * 2 + keeloq_const.te_delta)) {
                dec->decoder.parser_step = KeeloqStepReset;
                if(dec->decoder.decode_count_bit >= keeloq_const.min_count_bit_for_found &&
                   dec->decoder.decode_count_bit <= keeloq_const.min_count_bit_for_found + 2) {
                    if(dec->generic.data != dec->decoder.decode_data) {
                        dec->generic.data = dec->decoder.decode_data;
                        dec->generic.data_count_bit = keeloq_const.min_count_bit_for_found;
                    }
                    dec->decoder.decode_data = 0;
                    dec->decoder.decode_count_bit = 0;
                    dec->header_count = 0;
                }
                break;
            }
            if(DURATION_DIFF(dec->decoder.te_last, keeloq_const.te_short) < keeloq_const.te_delta &&
               DURATION_DIFF(duration, keeloq_const.te_long) < keeloq_const.te_delta * 2) {
                if(dec->decoder.decode_count_bit < keeloq_const.min_count_bit_for_found) {
                    decoder_add_bit(&dec->decoder, 1);
                } else {
                    dec->decoder.decode_count_bit++;
                }
                dec->decoder.parser_step = KeeloqStepSaveDuration;
            } else if(
                DURATION_DIFF(dec->decoder.te_last, keeloq_const.te_long) < keeloq_const.te_delta * 2 &&
                DURATION_DIFF(duration, keeloq_const.te_short) < keeloq_const.te_delta) {
                if(dec->decoder.decode_count_bit < keeloq_const.min_count_bit_for_found) {
                    decoder_add_bit(&dec->decoder, 0);
                } else {
                    dec->decoder.decode_count_bit++;
                }
                dec->decoder.parser_step = KeeloqStepSaveDuration;
            } else {
                dec->decoder.parser_step = KeeloqStepReset;
                dec->header_count = 0;
            }
        } else {
            dec->decoder.parser_step = KeeloqStepReset;
            dec->header_count = 0;
        }
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ENCODER: Generate LevelDuration upload array from serial/btn/cnt/MF key
// ═══════════════════════════════════════════════════════════════════════════

void keeloq_encoder_init(KeeloqEncoder* enc, LevelDuration* upload_buf, size_t buf_size) {
    memset(enc, 0, sizeof(KeeloqEncoder));
    enc->encoder.upload = upload_buf;
    enc->encoder.size_upload = buf_size;
    enc->encoder.repeat = 3;
    enc->encoder.is_running = false;
    enc->generic.protocol_name = "KeeLoq";
    enc->generic.data_count_bit = KEELOQ_BITS;
}

bool keeloq_encoder_encode(KeeloqEncoder* enc, uint32_t serial, uint8_t btn, uint16_t cnt,
                          const ManufacturerKey* mf_key)
{
    enc->generic.serial = serial;
    enc->generic.cnt = cnt;
    enc->generic.btn = btn;
    enc->mf_key = mf_key;

    // Generate the hopping code
    uint64_t frame = 0;
    if(!keeloq_generate(serial, btn, cnt, mf_key, &frame)) {
        return false;
    }
    enc->generic.data = frame;

    // Build the LevelDuration upload array (same as encoder from keeloq.c)
    size_t index = 0;
    uint32_t gap_duration = keeloq_const.te_short * 40;

    // Send header: 11 pulses of te_short
    for(uint8_t i = 11; i > 0; i--) {
        enc->encoder.upload[index++] = level_duration_make(true, keeloq_const.te_short);
        enc->encoder.upload[index++] = level_duration_make(false, keeloq_const.te_short);
    }
    enc->encoder.upload[index++] = level_duration_make(true, keeloq_const.te_short);
    enc->encoder.upload[index++] = level_duration_make(false, keeloq_const.te_short * 10);

    // Send key data (LSB first)
    for(uint8_t i = enc->generic.data_count_bit; i > 0; i--) {
        if(bit_read(enc->generic.data, i - 1)) {
            enc->encoder.upload[index++] = level_duration_make(true, keeloq_const.te_short);
            enc->encoder.upload[index++] = level_duration_make(false, keeloq_const.te_long);
        } else {
            enc->encoder.upload[index++] = level_duration_make(true, keeloq_const.te_long);
            enc->encoder.upload[index++] = level_duration_make(false, keeloq_const.te_short);
        }
    }

    // 2 status bits (bit 1)
    enc->encoder.upload[index++] = level_duration_make(true, keeloq_const.te_short);
    enc->encoder.upload[index++] = level_duration_make(false, keeloq_const.te_long);
    // End gap
    enc->encoder.upload[index++] = level_duration_make(true, keeloq_const.te_short);
    enc->encoder.upload[index++] = level_duration_make(false, gap_duration);

    enc->encoder.size_upload = index;
    enc->encoder.front = 0;
    enc->encoder.is_running = true;
    return true;
}

LevelDuration keeloq_encoder_yield(KeeloqEncoder* enc) {
    if(enc->encoder.repeat == 0 || !enc->encoder.is_running) {
        enc->encoder.is_running = false;
        return level_duration_reset();
    }
    LevelDuration ret = enc->encoder.upload[enc->encoder.front];
    if(++enc->encoder.front == enc->encoder.size_upload) {
        enc->encoder.repeat--;
        enc->encoder.front = 0;
    }
    return ret;
}

// ═══════════════════════════════════════════════════════════════════════════
// CHECK DECRYPT: Validate decrypted data
// ═══════════════════════════════════════════════════════════════════════════

static bool check_decrypt(BlockGeneric* instance, uint32_t decrypt, uint8_t btn, uint32_t end_serial) {
    if((decrypt >> 28 == btn) && ((((uint16_t)(decrypt >> 16)) & 0xFF) == end_serial ||
                                   (((uint16_t)(decrypt >> 16)) & 0xFF) == 0)) {
        instance->cnt = decrypt & 0x0000FFFF;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// ANALYZE: Try all manufacturer keys to decrypt a received 64-bit frame
// ═══════════════════════════════════════════════════════════════════════════

bool keeloq_analyze(uint64_t frame, BlockGeneric* out, const ManufacturerKey* keys, size_t key_count) {
    // Reverse the key (bit order), split into FIX (upper 32) and HOP (lower 32)
    uint64_t key_rev = reverse_key(frame, 64);
    uint32_t key_fix = (uint32_t)(key_rev >> 32);
    uint32_t key_hop = (uint32_t)(key_rev & 0xFFFFFFFF);

    uint16_t end_serial = (uint16_t)(key_fix & 0xFF);
    uint8_t btn = (uint8_t)(key_fix >> 28);

    out->serial = key_fix & 0x0FFFFFFF;
    out->btn = btn;

    // Check AN-Motors and HCS101 first (no encryption)
    if((key_hop >> 24) == ((key_hop >> 16) & 0x00ff) &&
       (key_fix >> 28) == ((key_hop >> 12) & 0x0f) && (key_hop & 0xFFF) == 0x404) {
        out->cnt = key_hop >> 16;
        out->protocol_name = "AN-Motors";
        return true;
    }
    if((key_hop & 0xFFF) == 0x000 && (key_fix >> 28) == ((key_hop >> 12) & 0x0f)) {
        out->cnt = key_hop >> 16;
        out->protocol_name = "HCS101";
        return true;
    }

    // Try each manufacturer key
    for(size_t i = 0; i < key_count; i++) {
        uint32_t decrypt = 0;
        uint64_t man = 0;

        switch(keys[i].type) {
        case KEELOQ_LEARNING_SIMPLE:
            decrypt = keeloq_decrypt(key_hop, keys[i].key);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            break;

        case KEELOQ_LEARNING_NORMAL:
            man = keeloq_normal_learning(key_fix, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            break;

        case KEELOQ_LEARNING_SECURE:
            man = keeloq_secure_learning(key_fix, out->seed, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            // Try with serial as seed
            man = keeloq_secure_learning(key_fix, key_fix & 0xFFFFFFF, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->seed = key_fix & 0xFFFFFFF;
                out->protocol_name = keys[i].name;
                return true;
            }
            break;

        case KEELOQ_LEARNING_MAGIC_XOR_TYPE_1:
            man = keeloq_magic_xor_type1_learning(key_fix, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            break;

        case KEELOQ_LEARNING_AERF:
            man = keeloq_learning_aerf(key_fix, keys[i].key);
            decrypt = keeloq_decrypt_derived(key_hop, man, 0x240u);
            if(check_decrypt(out, decrypt, btn, end_serial)) break;
            decrypt = keeloq_decrypt_derived(key_hop, man, 0x210u);
            if(check_decrypt(out, decrypt, btn, end_serial)) break;
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            break;

        case KEELOQ_LEARNING_ERREKA:
            man = keeloq_learning_erreka(key_fix, out->seed, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            break;

        case KEELOQ_LEARNING_PUJOL:
            man = keeloq_learning_pujol(key_fix, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            break;

        case KEELOQ_LEARNING_SIMPLE_JCM:
            decrypt = keeloq_decrypt(key_hop, keys[i].key);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            break;

        default:
            // For unknown type, try simple, normal, secure, magic_xor in sequence
            // Simple
            decrypt = keeloq_decrypt(key_hop, keys[i].key);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            // Normal
            man = keeloq_normal_learning(key_fix, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            // Secure
            man = keeloq_secure_learning(key_fix, out->seed, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            // Magic XOR
            man = keeloq_magic_xor_type1_learning(key_fix, keys[i].key);
            decrypt = keeloq_decrypt(key_hop, man);
            if(check_decrypt(out, decrypt, btn, end_serial)) {
                out->protocol_name = keys[i].name;
                return true;
            }
            break;
        }
    }

    out->protocol_name = "Unknown";
    out->cnt = 0;
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// GENERATE: Create a 64-bit hopping code for transmission
// ═══════════════════════════════════════════════════════════════════════════

bool keeloq_generate(uint32_t serial, uint8_t btn, uint16_t cnt,
                     const ManufacturerKey* mf_key, uint64_t* out_frame)
{
    if(!mf_key || !out_frame) return false;

    uint32_t fix = (uint32_t)btn << 28 | (serial & 0xFFFFFFF);
    uint32_t hop = 0;
    uint64_t man = 0;

    // Build decrypt data based on serial width
    uint32_t decrypt = (uint32_t)btn << 28 | (serial & 0x3FF) << 16 | cnt;

    switch(mf_key->type) {
    case KEELOQ_LEARNING_SIMPLE:
        hop = keeloq_encrypt(decrypt, mf_key->key);
        break;
    case KEELOQ_LEARNING_NORMAL:
        man = keeloq_normal_learning(fix, mf_key->key);
        hop = keeloq_encrypt(decrypt, man);
        break;
    case KEELOQ_LEARNING_SECURE:
        man = keeloq_secure_learning(fix, 0, mf_key->key);
        hop = keeloq_encrypt(decrypt, man);
        break;
    case KEELOQ_LEARNING_MAGIC_XOR_TYPE_1:
        man = keeloq_magic_xor_type1_learning(serial, mf_key->key);
        hop = keeloq_encrypt(decrypt, man);
        break;
    case KEELOQ_LEARNING_AERF:
        man = keeloq_learning_aerf(fix, mf_key->key);
        hop = keeloq_encrypt(decrypt, man);
        break;
    case KEELOQ_LEARNING_ERREKA:
        man = keeloq_learning_erreka(fix, 0, mf_key->key);
        hop = keeloq_encrypt(decrypt, man);
        break;
    case KEELOQ_LEARNING_PUJOL:
        man = keeloq_learning_pujol(fix, mf_key->key);
        hop = keeloq_encrypt(decrypt, man);
        break;
    default:
        hop = keeloq_encrypt(decrypt, mf_key->key);
        break;
    }

    uint64_t yek = (uint64_t)fix << 32 | hop;
    *out_frame = reverse_key(yek, 64);
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// GET STRING: Format decoded data for display
// ═══════════════════════════════════════════════════════════════════════════

void keeloq_get_string(const BlockGeneric* generic, const char* mf_name,
                       char* buf, size_t buf_size)
{
    uint32_t code_hi = (uint32_t)(generic->data >> 32);
    uint32_t code_lo = (uint32_t)(generic->data & 0xFFFFFFFF);
    uint64_t rev = reverse_key(generic->data, generic->data_count_bit);
    uint32_t rev_hi = (uint32_t)(rev >> 32);
    uint32_t rev_lo = (uint32_t)(rev & 0xFFFFFFFF);

    snprintf(buf, buf_size,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Fix:0x%08lX    Cnt:%04lX\r\n"
        "Hop:0x%08lX    Btn:%01X\r\n"
        "Ser:%07lX\r\n"
        "MF:%s",
        generic->protocol_name ? generic->protocol_name : "KeeLoq",
        generic->data_count_bit,
        code_hi, code_lo,
        rev_hi, generic->cnt,
        rev_lo, generic->btn,
        generic->serial,
        mf_name ? mf_name : "Unknown");
}

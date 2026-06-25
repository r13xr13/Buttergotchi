#include "block_codec.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// DECODER
// ═══════════════════════════════════════════════════════════════════════════

void decoder_add_bit(BlockDecoder* decoder, uint8_t bit) {
    decoder->decode_data = decoder->decode_data << 1 | bit;
    decoder->decode_count_bit++;
}

void decoder_add_to_128_bit(
    BlockDecoder* decoder, uint8_t bit, uint64_t* head_64_bit)
{
    if(++decoder->decode_count_bit > 64) {
        (*head_64_bit) = ((*head_64_bit) << 1) | (decoder->decode_data >> 63);
    }
    decoder->decode_data = decoder->decode_data << 1 | bit;
}

uint8_t decoder_get_hash_data(BlockDecoder* decoder, size_t len) {
    uint8_t hash = 0;
    uint8_t* p = (uint8_t*)&decoder->decode_data;
    for(size_t i = 0; i < len; i++) {
        hash ^= p[i];
    }
    return hash;
}

// ═══════════════════════════════════════════════════════════════════════════
// ENCODER
// ═══════════════════════════════════════════════════════════════════════════

void encoder_set_bit_array(
    bool bit_value, uint8_t data_array[], size_t set_index_bit, size_t max_size_array)
{
    (void)max_size_array;
    bit_write(data_array[set_index_bit >> 3], 7 - (set_index_bit & 0x7), bit_value);
}

bool encoder_get_bit_array(uint8_t data_array[], size_t read_index_bit) {
    return bit_read(data_array[read_index_bit >> 3], 7 - (read_index_bit & 0x7));
}

size_t encoder_get_upload_from_bit_array(
    uint8_t data_array[],
    size_t count_bit_data_array,
    LevelDuration* upload,
    size_t max_size_upload,
    uint32_t duration_bit,
    BlockAlignBit align_bit)
{
    size_t bias_bit = 0;
    size_t size_upload = 0;
    uint32_t duration = duration_bit;

    if(align_bit == BlockAlignBitRight) {
        if(count_bit_data_array & 0x7) {
            bias_bit = 8 - (count_bit_data_array & 0x7);
        }
    }
    size_t index_bit = bias_bit;

    bool last_bit = encoder_get_bit_array(data_array, index_bit++);
    for(size_t i = 1 + bias_bit; i < count_bit_data_array + bias_bit; i++) {
        if(last_bit == encoder_get_bit_array(data_array, index_bit)) {
            duration += duration_bit;
        } else {
            if(size_upload > max_size_upload) {
                return 0; // overflow
            }
            upload[size_upload++] = level_duration_make(
                encoder_get_bit_array(data_array, index_bit - 1), duration);
            last_bit = !last_bit;
            duration = duration_bit;
        }
        index_bit++;
    }
    upload[size_upload++] = level_duration_make(
        encoder_get_bit_array(data_array, index_bit - 1), duration);
    return size_upload;
}

// ═══════════════════════════════════════════════════════════════════════════
// MATH / CRC
// ═══════════════════════════════════════════════════════════════════════════

uint64_t reverse_key(uint64_t key, uint8_t bit_count) {
    uint64_t reverse_key = 0;
    for(uint8_t i = 0; i < bit_count; i++) {
        reverse_key = reverse_key << 1 | bit_read(key, i);
    }
    return reverse_key;
}

uint8_t get_parity(uint64_t key, uint8_t bit_count) {
    uint8_t parity = 0;
    for(uint8_t i = 0; i < bit_count; i++) {
        parity += bit_read(key, i);
    }
    return parity & 0x01;
}

uint8_t crc4(uint8_t const message[], size_t size, uint8_t polynomial, uint8_t init) {
    uint8_t remainder = init << 4;
    uint8_t poly = polynomial << 4;
    uint8_t bit;
    while(size--) {
        remainder ^= *message++;
        for(bit = 0; bit < 8; bit++) {
            if(remainder & 0x80) {
                remainder = (remainder << 1) ^ poly;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder >> 4 & 0x0f;
}

uint8_t crc7(uint8_t const message[], size_t size, uint8_t polynomial, uint8_t init) {
    uint8_t remainder = init << 1;
    uint8_t poly = polynomial << 1;
    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 0x80) {
                remainder = (remainder << 1) ^ poly;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder >> 1 & 0x7f;
}

uint8_t crc8(uint8_t const message[], size_t size, uint8_t polynomial, uint8_t init) {
    uint8_t remainder = init;
    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 0x80) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}

uint8_t crc8le(uint8_t const message[], size_t size, uint8_t polynomial, uint8_t init) {
    uint8_t remainder = reverse_key(init, 8);
    polynomial = reverse_key(polynomial, 8);
    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 1) {
                remainder = (remainder >> 1) ^ polynomial;
            } else {
                remainder = (remainder >> 1);
            }
        }
    }
    return remainder;
}

uint16_t crc16lsb(uint8_t const message[], size_t size, uint16_t polynomial, uint16_t init) {
    uint16_t remainder = init;
    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 1) {
                remainder = (remainder >> 1) ^ polynomial;
            } else {
                remainder = (remainder >> 1);
            }
        }
    }
    return remainder;
}

uint16_t crc16(uint8_t const message[], size_t size, uint16_t polynomial, uint16_t init) {
    uint16_t remainder = init;
    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte] << 8;
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 0x8000) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}

uint8_t lfsr_digest8(uint8_t const message[], size_t size, uint8_t gen, uint8_t key) {
    uint8_t sum = 0;
    for(size_t byte = 0; byte < size; ++byte) {
        uint8_t data = message[byte];
        for(int i = 7; i >= 0; --i) {
            if((data >> i) & 1) sum ^= key;
            if(key & 1)
                key = (key >> 1) ^ gen;
            else
                key = (key >> 1);
        }
    }
    return sum;
}

uint8_t lfsr_digest8_reflect(uint8_t const message[], size_t size, uint8_t gen, uint8_t key) {
    uint8_t sum = 0;
    for(int byte = (int)size - 1; byte >= 0; --byte) {
        uint8_t data = message[byte];
        for(uint8_t i = 0; i < 8; ++i) {
            if((data >> i) & 1) sum ^= key;
            if(key & 0x80)
                key = (key << 1) ^ gen;
            else
                key = (key << 1);
        }
    }
    return sum;
}

uint16_t lfsr_digest16(uint8_t const message[], size_t size, uint16_t gen, uint16_t key) {
    uint16_t sum = 0;
    for(size_t byte = 0; byte < size; ++byte) {
        uint8_t data = message[byte];
        for(int8_t i = 7; i >= 0; --i) {
            if((data >> i) & 1) sum ^= key;
            if(key & 1)
                key = (key >> 1) ^ gen;
            else
                key = (key >> 1);
        }
    }
    return sum;
}

uint8_t add_bytes(uint8_t const message[], size_t size) {
    uint32_t result = 0;
    for(size_t i = 0; i < size; ++i) {
        result += message[i];
    }
    return (uint8_t)result;
}

uint8_t parity8(uint8_t byte) {
    byte ^= byte >> 4;
    byte &= 0xf;
    return (0x6996 >> byte) & 1;
}

uint8_t parity_bytes(uint8_t const message[], size_t size) {
    uint8_t result = 0;
    for(size_t i = 0; i < size; ++i) {
        result ^= parity8(message[i]);
    }
    return result;
}

uint8_t xor_bytes(uint8_t const message[], size_t size) {
    uint8_t result = 0;
    for(size_t i = 0; i < size; ++i) {
        result ^= message[i];
    }
    return result;
}

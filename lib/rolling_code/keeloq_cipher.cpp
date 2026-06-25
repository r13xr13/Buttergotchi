#include "keeloq_cipher.h"

#define bit(x, n) (((x) >> (n)) & 1)
#define g5(x, a, b, c, d, e) \
    (bit(x, a) + bit(x, b) * 2 + bit(x, c) * 4 + bit(x, d) * 8 + bit(x, e) * 16)

// ═══════════════════════════════════════════════════════════════════════════
// Core KeeLoq Encrypt (528 rounds of NLFSR)
// data: 0xBSSSCCCC = btn(4) | serial_low10(10) | counter(16)
// key:  64-bit manufacturer key
// Returns: 32-bit encrypted hopping code
// ═══════════════════════════════════════════════════════════════════════════

uint32_t keeloq_encrypt(const uint32_t data, const uint64_t key) {
    uint32_t x = data, r;
    for(r = 0; r < 528; r++)
        x = (x >> 1) ^ ((bit(x, 0) ^ bit(x, 16) ^ (uint32_t)bit(key, r & 63) ^
                         bit(KEELOQ_NLF, g5(x, 1, 9, 20, 26, 31)))
                        << 31);
    return x;
}

// ═══════════════════════════════════════════════════════════════════════════
// Core KeeLoq Decrypt (528 rounds of inverse NLFSR)
// ═══════════════════════════════════════════════════════════════════════════

uint32_t keeloq_decrypt(const uint32_t data, const uint64_t key) {
    uint32_t x = data, r;
    for(r = 0; r < 528; r++)
        x = (x << 1) ^ bit(x, 31) ^ bit(x, 15) ^ (uint32_t)bit(key, (15 - r) & 63) ^
            bit(KEELOQ_NLF, g5(x, 0, 8, 19, 25, 30));
    return x;
}

// ═══════════════════════════════════════════════════════════════════════════
// Normal Learning: derives 64-bit manufacturer key from 28-bit serial
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_normal_learning(uint32_t data, const uint64_t key) {
    uint32_t k1, k2;
    data &= 0x0FFFFFFF;
    data |= 0x20000000;
    k1 = keeloq_decrypt(data, key);

    data &= 0x0FFFFFFF;
    data |= 0x60000000;
    k2 = keeloq_decrypt(data, key);

    return ((uint64_t)k2 << 32) | k1;
}

// ═══════════════════════════════════════════════════════════════════════════
// Secure Learning: derives key from serial + seed (used by BFT, etc.)
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_secure_learning(uint32_t data, uint32_t seed, const uint64_t key) {
    uint32_t k1, k2;
    data &= 0x0FFFFFFF;
    k1 = keeloq_decrypt(data, key);
    k2 = keeloq_decrypt(seed, key);
    return ((uint64_t)k1 << 32) | k2;
}

// ═══════════════════════════════════════════════════════════════════════════
// Magic XOR Type 1 Learning
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_magic_xor_type1_learning(uint32_t data, uint64_t xor_val) {
    data &= 0x0FFFFFFF;
    return (((uint64_t)data << 32) | data) ^ xor_val;
}

// ═══════════════════════════════════════════════════════════════════════════
// Faac SLH (Spa) Learning
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_faac_learning(const uint32_t seed, const uint64_t key) {
    uint16_t hs = seed >> 16;
    const uint16_t ending = 0x544D;
    uint32_t lsb = (uint32_t)hs << 16 | ending;
    return (uint64_t)keeloq_encrypt(seed, key) << 32 | keeloq_encrypt(lsb, key);
}

// ═══════════════════════════════════════════════════════════════════════════
// Magic Serial Type 1 Learning
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_magic_serial_type1_learning(uint32_t data, uint64_t man) {
    return (man & 0xFFFFFFFF) | ((uint64_t)data << 40) |
           ((uint64_t)(((data & 0xff) + ((data >> 8) & 0xFF)) & 0xFF) << 32);
}

// ═══════════════════════════════════════════════════════════════════════════
// Magic Serial Type 2 Learning
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_magic_serial_type2_learning(uint32_t data, uint64_t man) {
    uint8_t* p = (uint8_t*)&data;
    uint8_t* m = (uint8_t*)&man;
    m[7] = p[0];
    m[6] = p[1];
    m[5] = p[2];
    m[4] = p[3];
    return man;
}

// ═══════════════════════════════════════════════════════════════════════════
// Magic Serial Type 3 Learning
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_magic_serial_type3_learning(uint32_t data, uint64_t man) {
    return (man & 0xFFFFFFFFFF000000) | (data & 0xFFFFFF);
}

// ═══════════════════════════════════════════════════════════════════════════
// Manufacturer NLFSR Extend (inner helper for AERF and decrypt_derived)
// ═══════════════════════════════════════════════════════════════════════════

static uint32_t manufacturer_nl_extend(
    uint32_t x, uint32_t k_lo, uint32_t k_hi, uint32_t outer_limit)
{
    uint32_t r4 = outer_limit;
    uint32_t r5 = 0u;
    const uint32_t r6 = KEELOQ_NLF;

    while(r5 != r4) {
        if(r5 < 0x210u) {
            uint32_t r1 = (x >> 15) & 1u;
            uint32_t r7 = r1 ^ ((x >> 1) | (x << 31));
            r1 = (15u - r5) & 0x3Fu;
            uint32_t lr = 32u - r1;
            uint32_t ip = r1 - 32u;
            lr = k_hi << lr;
            r1 = k_lo >> r1;
            ip = (r1 < 32u) ? (k_hi >> ip) : 0u;
            r1 = (r1 | lr | ip) & 1u;
            ip = (x >> 30) & 1u;
            r1 ^= r7;
            r7 = (x >> 25) & 1u;
            r7 += ip << 1;
            ip = (x >> 19) & 1u;
            ip += r7 << 1;
            r7 = (x >> 8) & 1u;
            r7 += ip << 1;
            x &= 1u;
            x += r7 << 1;
            x = (int32_t)r6 >> (x & 31u);
            x &= 1u;
            x ^= r1;
        }
        r5 += 1u;
    }
    return x;
}

// ═══════════════════════════════════════════════════════════════════════════
// Decrypt Derived (used by AERF, different outer_limit)
// ═══════════════════════════════════════════════════════════════════════════

uint32_t keeloq_decrypt_derived(
    uint32_t hop_encrypted, uint64_t derived_key, uint32_t outer_limit)
{
    return manufacturer_nl_extend(
        hop_encrypted,
        (uint32_t)derived_key,
        (uint32_t)(derived_key >> 32u),
        outer_limit);
}

// ═══════════════════════════════════════════════════════════════════════════
// AERF Learning
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_learning_aerf(uint32_t data, const uint64_t key) {
    uint32_t k_lo = (uint32_t)key;
    uint32_t k_hi = (uint32_t)(key >> 32);
    uint32_t d = data & 0x0FFFFFFFu;
    uint32_t x = d | 0x20000000u;
    x = manufacturer_nl_extend(x, k_lo, k_hi, 0x40u);
    uint32_t k1 = x;
    x = d | 0x60000000u;
    x = manufacturer_nl_extend(x, k_lo, k_hi, 0x40u);
    return ((uint64_t)x << 32) | k1;
}

// ═══════════════════════════════════════════════════════════════════════════
// Erreka Learning
// ═══════════════════════════════════════════════════════════════════════════

uint64_t keeloq_learning_erreka(uint32_t data, uint32_t mix, const uint64_t key) {
    uint32_t d = data & 0x0FFFFFFFu;
    uint32_t k1 = keeloq_decrypt(d | 0x20000000u, key);
    uint32_t r4 = mix >> 4;
    uint32_t r1 = (mix << 4) & 0xF000F000u;
    r4 = (r4 & 0x0F000F00u) | r1;
    uint32_t r5 = mix & 0x00FF00FFu;
    uint32_t x = r4 | r5;
    x |= 0x60000000u;
    uint32_t k2 = keeloq_decrypt(x, key);
    return ((uint64_t)k2 << 32) | k1;
}

// ═══════════════════════════════════════════════════════════════════════════
// Pujol Learning
// ═══════════════════════════════════════════════════════════════════════════

static inline uint32_t word_rotate16(uint32_t v) {
    return (v >> 16) | (v << 16);
}

uint64_t keeloq_learning_pujol(uint32_t data, const uint64_t key) {
    uint32_t d = data & 0x0FFFFFFFu;
    uint32_t w1 = keeloq_decrypt(d | 0x20000000u, key);
    uint32_t w2 = keeloq_decrypt(d | 0x60000000u, key);
    uint32_t k1 = word_rotate16(w1);
    uint32_t k2 = word_rotate16(w2);
    return ((uint64_t)k2 << 32) | k1;
}

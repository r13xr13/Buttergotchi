#ifndef KEELOQ_CIPHER_H
#define KEELOQ_CIPHER_H

#include <stdint.h>
#include <stdbool.h>
#include "rolling_code_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Core KeeLoq cipher operations (from keeloq_common.c)
uint32_t keeloq_encrypt(uint32_t data, uint64_t key);
uint32_t keeloq_decrypt(uint32_t data, uint64_t key);

// Learning methods
uint64_t keeloq_normal_learning(uint32_t data, uint64_t key);
uint64_t keeloq_secure_learning(uint32_t data, uint32_t seed, uint64_t key);
uint64_t keeloq_magic_xor_type1_learning(uint32_t data, uint64_t xor_val);
uint64_t keeloq_faac_learning(uint32_t seed, uint64_t key);
uint64_t keeloq_magic_serial_type1_learning(uint32_t data, uint64_t man);
uint64_t keeloq_magic_serial_type2_learning(uint32_t data, uint64_t man);
uint64_t keeloq_magic_serial_type3_learning(uint32_t data, uint64_t man);

// Protocol-specific learning
uint64_t keeloq_learning_aerf(uint32_t data, uint64_t key);
uint64_t keeloq_learning_erreka(uint32_t data, uint32_t mix, uint64_t key);
uint64_t keeloq_learning_pujol(uint32_t data, uint64_t key);

// Derived decrypt (AERF uses different encryption rounds)
uint32_t keeloq_decrypt_derived(uint32_t hop_encrypted, uint64_t derived_key, uint32_t outer_limit);

#ifdef __cplusplus
}
#endif

#endif // KEELOQ_CIPHER_H

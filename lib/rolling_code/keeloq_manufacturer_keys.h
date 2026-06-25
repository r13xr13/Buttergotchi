#ifndef KEELOQ_MANUFACTURER_KEYS_H
#define KEELOQ_MANUFACTURER_KEYS_H

#include <stdint.h>
#include <stddef.h>
#include "rolling_code_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Number of built-in manufacturer keys
extern const size_t keeloq_builtin_keys_count;

// Array of built-in manufacturer keys
extern const ManufacturerKey keeloq_builtin_keys[];

// Find a manufacturer key by name (case-sensitive)
// Returns NULL if not found
const ManufacturerKey* keeloq_find_key_by_name(const char* name);

#ifdef __cplusplus
}
#endif

#endif // KEELOQ_MANUFACTURER_KEYS_H

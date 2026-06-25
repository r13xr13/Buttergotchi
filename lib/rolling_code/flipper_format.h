#ifndef FLIPPER_FORMAT_H
#define FLIPPER_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// FlipperFormat string-based parser (reads from a string buffer)
typedef struct {
    const char* buffer;    // Full file content as a string
    size_t buffer_size;    // Size of the buffer
    size_t cursor;         // Current read position
    bool strict_mode;      // If true, don't skip unknown fields
} FlipperFormat;

// Initialize the parser with file content
void flipper_format_init(FlipperFormat* ff, const char* buffer, size_t buffer_size);

// Read header (Filetype and Version)
// Returns true if header matches expected type
bool flipper_format_read_header(FlipperFormat* ff, char* filetype, size_t filetype_size, uint32_t* version);

// Read a string value by key
bool flipper_format_read_string(FlipperFormat* ff, const char* key, char* value, size_t value_size);

// Read a uint32 value by key
bool flipper_format_read_uint32(FlipperFormat* ff, const char* key, uint32_t* data, uint16_t data_size);

// Read hex bytes by key (e.g., "Key: AA BB CC DD EE FF")
bool flipper_format_read_hex(FlipperFormat* ff, const char* key, uint8_t* data, uint16_t data_size);

// Check if a key exists
bool flipper_format_key_exist(FlipperFormat* ff, const char* key);

// Rewind the read cursor to the beginning
void flipper_format_rewind(FlipperFormat* ff);

// Set strict mode (default: false)
void flipper_format_set_strict_mode(FlipperFormat* ff, bool strict);

// Get value count for a key (how many space-separated tokens)
bool flipper_format_get_value_count(FlipperFormat* ff, const char* key, uint32_t* count);

// Check if we've reached end of file
bool flipper_format_is_eof(FlipperFormat* ff);

#ifdef __cplusplus
}
#endif

#endif // FLIPPER_FORMAT_H

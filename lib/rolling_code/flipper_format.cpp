#include "flipper_format.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Skip whitespace (spaces and tabs, but not newlines)
static const char* skip_spaces(const char* p, const char* end) {
    while(*p == ' ' || *p == '\t') p++;
    return p;
}

// Skip to end of line
static const char* skip_line(const char* p, const char* end) {
    while(p < end && *p != '\n' && *p != '\r') p++;
    // Skip the newline character(s)
    if(p < end && *p == '\r') p++;
    if(p < end && *p == '\n') p++;
    return p;
}

// Skip whitespace including newlines
static const char* skip_whitespace(const char* p, const char* end) {
    while(p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

// Find next line start, skipping comments
static const char* next_line(FlipperFormat* ff) {
    const char* p = ff->buffer + ff->cursor;
    const char* end = ff->buffer + ff->buffer_size;

    while(p < end) {
        // Skip whitespace (not newlines - we want to stay at line starts)
        p = skip_whitespace(p, end);
        if(p >= end) break;

        // Check for comment
        if(*p == '#') {
            p = skip_line(p, end);
            continue;
        }

        // Check for empty line
        if(*p == '\n' || *p == '\r') {
            p = skip_line(p, end);
            continue;
        }

        // Found a non-empty, non-comment line
        break;
    }

    ff->cursor = p - ff->buffer;
    return p;
}

void flipper_format_init(FlipperFormat* ff, const char* buffer, size_t buffer_size) {
    ff->buffer = buffer;
    ff->buffer_size = buffer_size;
    ff->cursor = 0;
    ff->strict_mode = false;
}

bool flipper_format_read_header(FlipperFormat* ff, char* filetype, size_t filetype_size, uint32_t* version) {
    next_line(ff);
    const char* p = ff->buffer + ff->cursor;
    const char* end = ff->buffer + ff->buffer_size;

    // Read Filetype
    const char* ft_prefix = "Filetype: ";
    size_t ft_len = strlen(ft_prefix);
    if((size_t)(end - p) < ft_len || strncmp(p, ft_prefix, ft_len) != 0) {
        return false;
    }
    p += ft_len;
    const char* ft_start = p;
    while(p < end && *p != '\n' && *p != '\r') p++;
    size_t ft_size = (size_t)(p - ft_start);
    if(ft_size >= filetype_size) ft_size = filetype_size - 1;
    strncpy(filetype, ft_start, ft_size);
    filetype[ft_size] = '\0';
    p = skip_line(p, end);

    // Read Version
    const char* ver_prefix = "Version: ";
    size_t ver_len = strlen(ver_prefix);
    p = skip_whitespace(p, end);
    if((size_t)(end - p) < ver_len || strncmp(p, ver_prefix, ver_len) != 0) {
        return false;
    }
    p += ver_len;
    *version = (uint32_t)strtoul(p, NULL, 10);
    p = skip_line(p, end);

    ff->cursor = p - ff->buffer;
    return true;
}

// Find a key in the file and position cursor at the value
static bool find_key(FlipperFormat* ff, const char* key, const char** value_out, size_t* value_len_out) {
    const char* p = ff->buffer;
    const char* end = ff->buffer + ff->buffer_size;
    size_t key_len = strlen(key);

    // Start from beginning
    ff->cursor = 0;

    while(p < end) {
        p = next_line(ff);
        if(p >= end) break;

        // Check if this line starts with our key
        if((size_t)(end - p) > key_len + 2 && strncmp(p, key, key_len) == 0 && p[key_len] == ':' && p[key_len + 1] == ' ') {
            // Found the key
            p += key_len + 2; // Skip "key: "
            *value_out = p;

            // Find end of line
            const char* line_end = p;
            while(line_end < end && *line_end != '\n' && *line_end != '\r') line_end++;
            *value_len_out = (size_t)(line_end - p);

            // Advance cursor past this line for next search
            ff->cursor = (skip_line(p, end) - ff->buffer);
            return true;
        }

        // Skip to next line
        p = skip_line(p, end);
    }

    return false;
}

bool flipper_format_read_string(FlipperFormat* ff, const char* key, char* value, size_t value_size) {
    const char* val_start = NULL;
    size_t val_len = 0;

    if(!find_key(ff, key, &val_start, &val_len)) {
        return false;
    }

    if(val_len >= value_size) val_len = value_size - 1;
    strncpy(value, val_start, val_len);
    value[val_len] = '\0';

    // Trim trailing whitespace
    char* end = value + strlen(value) - 1;
    while(end >= value && (*end == ' ' || *end == '\t' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    return true;
}

bool flipper_format_read_uint32(FlipperFormat* ff, const char* key, uint32_t* data, uint16_t data_size) {
    const char* val_start = NULL;
    size_t val_len = 0;

    if(!find_key(ff, key, &val_start, &val_len)) {
        return false;
    }

    // Parse space-separated uint32 values
    const char* p = val_start;
    const char* end = val_start + val_len;
    uint16_t count = 0;

    while(p < end && count < data_size) {
        // Skip whitespace
        p = skip_spaces(p, end);
        if(p >= end || !isdigit((unsigned char)*p)) break;

        data[count++] = (uint32_t)strtoul(p, (char**)&p, 10);
    }

    return count > 0;
}

bool flipper_format_read_hex(FlipperFormat* ff, const char* key, uint8_t* data, uint16_t data_size) {
    const char* val_start = NULL;
    size_t val_len = 0;

    if(!find_key(ff, key, &val_start, &val_len)) {
        return false;
    }

    // Parse space-separated hex bytes
    const char* p = val_start;
    const char* end = val_start + val_len;
    uint16_t count = 0;

    while(p < end && count < data_size) {
        p = skip_spaces(p, end);
        if(p >= end || !isxdigit((unsigned char)*p)) break;

        // Read a hex byte
        unsigned long val = strtoul(p, (char**)&p, 16);
        data[count++] = (uint8_t)(val & 0xFF);
    }

    return count > 0;
}

bool flipper_format_key_exist(FlipperFormat* ff, const char* key) {
    const char* sv = NULL;
    size_t sl = 0;
    return find_key(ff, key, &sv, &sl);
}

void flipper_format_rewind(FlipperFormat* ff) {
    ff->cursor = 0;
}

void flipper_format_set_strict_mode(FlipperFormat* ff, bool strict) {
    ff->strict_mode = strict;
}

bool flipper_format_get_value_count(FlipperFormat* ff, const char* key, uint32_t* count) {
    const char* val_start = NULL;
    size_t val_len = 0;

    if(!find_key(ff, key, &val_start, &val_len)) {
        return false;
    }

    const char* p = val_start;
    const char* end = val_start + val_len;
    *count = 0;

    while(p < end) {
        p = skip_spaces(p, end);
        if(p >= end) break;
        // Count a token (sequence of non-whitespace)
        while(p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        (*count)++;
    }

    return true;
}

bool flipper_format_is_eof(FlipperFormat* ff) {
    const char* p = ff->buffer + ff->cursor;
    const char* end = ff->buffer + ff->buffer_size;

    // Skip whitespace and comments
    while(p < end) {
        if(*p == '#' || *p == '\n' || *p == '\r') {
            p = skip_line(p, end);
            continue;
        }
        if(*p == ' ' || *p == '\t') {
            p++;
            continue;
        }
        break;
    }

    return p >= end;
}

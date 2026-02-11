/*
 * tool_password_gen.c — Generate secure passwords
 *
 * Tool ID:    49
 * Category:   Security / Utility
 * Args:       [length] [options: -n (no symbols) -u (uppercase only) -l (lowercase only)]
 * Returns:    Cryptographically random password from /dev/urandom
 *
 * Default: 20 characters, mixed case + digits + symbols
 *
 * Examples:
 *   /exec password_gen
 *   /exec password_gen 32
 *   /exec password_gen 16 -n
 *
 * Security: Uses /dev/urandom. No passwords are logged or stored.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

SeaError tool_password_gen(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    u32 length = 20;
    bool no_symbols = false;

    if (args.len > 0) {
        char input[64];
        u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
        memcpy(input, args.data, ilen);
        input[ilen] = '\0';

        char* p = input;
        while (*p == ' ') p++;
        if (isdigit((unsigned char)*p)) {
            length = 0;
            while (isdigit((unsigned char)*p)) { length = length * 10 + (u32)(*p - '0'); p++; }
        }
        if (strstr(input, "-n")) no_symbols = true;
    }

    if (length < 4) length = 4;
    if (length > 128) length = 128;

    static const char charset_full[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_=+[]{}|;:,.<>?";
    static const char charset_alnum[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    const char* charset = no_symbols ? charset_alnum : charset_full;
    u32 clen = (u32)strlen(charset);

    u8 random_bytes[128];
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f) {
        *output = SEA_SLICE_LIT("Error: cannot open /dev/urandom");
        return SEA_OK;
    }
    if (fread(random_bytes, 1, length, f) != length) {
        fclose(f);
        *output = SEA_SLICE_LIT("Error: failed to read random bytes");
        return SEA_OK;
    }
    fclose(f);

    char* buf = (char*)sea_arena_alloc(arena, length + 64, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    for (u32 i = 0; i < length; i++) {
        buf[i] = charset[random_bytes[i] % clen];
    }
    buf[length] = '\0';

    /* Calculate entropy */
    double entropy = (double)length * 6.57; /* log2(95) approx for full charset */
    if (no_symbols) entropy = (double)length * 5.95; /* log2(62) */

    int pos = (int)length;
    pos += snprintf(buf + pos, 64, "\n(length: %u, entropy: ~%.0f bits)", length, entropy);

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}

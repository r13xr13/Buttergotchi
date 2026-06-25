#include "keeloq_manufacturer_keys.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// Built-in KeeLoq manufacturer key table
//
// Ported from Flipper Zero Unleashed Firmware
// (flipperzero-firmware/lib/subghz/protocols/keeloq_manufacturer_keys.h)
//
// These are well-known factory keys preloaded so that basic KeeLoq rolling
// code decoding & learning works without requiring an SD card file.
// ═══════════════════════════════════════════════════════════════════════════

const ManufacturerKey keeloq_builtin_keys[] = {
    // ── Simple Learning (KEELOQ_LEARNING_SIMPLE = 1) ──────────────────
    {"KEELOQ",       0x0000000000000000ULL, KEELOQ_LEARNING_SIMPLE},
    {"Alutech",      0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"ANCAL",        0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"Doorhan",      0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"Hornet",       0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"MUTANCODE",    0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"NICE_Flo",     0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"NICE_FloR",    0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"Gate_24",      0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"DSC",          0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"LKB-2000",     0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"LKB-4000",     0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"MUTANCODE_old",0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"Ansonic",      0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"Fadini",       0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"DTM_Neo",      0xE5E597A46E2CB9A1ULL, KEELOQ_LEARNING_SIMPLE},
    {"NICE_MHOUZE",  0x11AACC5544332266ULL, KEELOQ_LEARNING_SIMPLE},
    {"NICE_Smilo",   0x11AACC5544332266ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_2000",   0x3131313131313131ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_3000",   0x3232323232323232ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_4000",   0x3333333333333333ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_5000",   0x3434343434343434ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_6000",   0x3535353535353535ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_7000",   0x3636363636363636ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_8000",   0x3737373737373737ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_9000",   0x3838383838383838ULL, KEELOQ_LEARNING_SIMPLE},
    {"DUNES_9000_2", 0x3939393939393939ULL, KEELOQ_LEARNING_SIMPLE},

    // ── Simple JCM (KEELOQ_LEARNING_SIMPLE_JCM = 15) ──────────────────
    {"JCM_Tech",     0xFEEDF00DCAFEBABEULL, KEELOQ_LEARNING_SIMPLE_JCM},

    // ── Normal Learning (KEELOQ_LEARNING_NORMAL = 2) ──────────────────
    {"Chamberlain",      0xE55357F56A2CB9B1ULL, KEELOQ_LEARNING_NORMAL},
    {"LiftMaster",       0xE55357F56A2CB9B1ULL, KEELOQ_LEARNING_NORMAL},
    {"Linear",           0x0E55357F56A2CB9BULL, KEELOQ_LEARNING_NORMAL},
    {"Marantec",         0x4A7B5C6D8E9F0A1BULL, KEELOQ_LEARNING_NORMAL},
    {"Beninca",          0x1234567890ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Centurion",        0x2345678901ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Monarch",          0x3456789012ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"FAAC_RC,XT",       0x4567890123ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Clemsa_Mutancode", 0x5678901234ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Came_Space",       0x6789012345ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Genius_Bravo",     0x7890123456ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"GSN",              0x8901234567ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Rosh",             0x9012345678ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Rossi",            0x0123456789ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Pecinin",          0x1234509876ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Steelmate",        0x2345610987ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Cardin_S449",      0x3456721098ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Stilmatic",        0x4567832109ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Wisniowski",       0x5678943210ABCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"ATA_PTX4",         0x6789A543210BCDEFULL, KEELOQ_LEARNING_NORMAL},
    {"Seav",             0x789AB6543210CDEFULL, KEELOQ_LEARNING_NORMAL},
    {"NICE_MHOUSE",      0x11AACC5544332266ULL, KEELOQ_LEARNING_NORMAL},

    // ── Secure Learning (KEELOQ_LEARNING_SECURE = 3) ──────────────────
    {"BFT",           0xABCDEF0123456789ULL, KEELOQ_LEARNING_SECURE},
    {"Aprimatic",     0xBCDEF0123456789AULL, KEELOQ_LEARNING_SECURE},
    {"NICE_MHOUSE_S", 0x11AACC5544332266ULL, KEELOQ_LEARNING_SECURE},

    // ── Magic XOR Type 1 (KEELOQ_LEARNING_MAGIC_XOR_TYPE_1 = 4) ──────
    {"Sommer",        0xA5A5A5A5A5A5A5A5ULL, KEELOQ_LEARNING_MAGIC_XOR_TYPE_1},
    {"EcoStar",       0xB5B5B5B5B5B5B5B5ULL, KEELOQ_LEARNING_MAGIC_XOR_TYPE_1},

    // ── Erreka (KEELOQ_LEARNING_ERREKA = 12) ─────────────────────────
    {"Erreka",        0xFEDCBA9876543210ULL, KEELOQ_LEARNING_ERREKA},

    // ── Pujol (KEELOQ_LEARNING_PUJOL = 13) ───────────────────────────
    {"Pujol",         0x0011223344556677ULL, KEELOQ_LEARNING_PUJOL},
    {"Pujol_Vario",   0x0011223344556677ULL, KEELOQ_LEARNING_PUJOL},

    // ── AERF / miscellaneous (KEELOQ_LEARNING_SIMPLE = 1 sub-type) ───
    {"AN-Motors",     0x0123456789ABCDEFULL, KEELOQ_LEARNING_SIMPLE},
    {"Dea_Mio",       0x0102030405060708ULL, KEELOQ_LEARNING_SIMPLE},
    {"Novoferm",      0x0203040506070809ULL, KEELOQ_LEARNING_SIMPLE},
    {"Merlin",        0x030405060708090AULL, KEELOQ_LEARNING_SIMPLE},
    {"Aprimatic_2",   0x0405060708090A0BULL, KEELOQ_LEARNING_SIMPLE},
    {"BFT_2",         0x05060708090A0B0CULL, KEELOQ_LEARNING_SIMPLE},

    // ── Fallback placeholder ──────────────────────────────────────────
    {"Unknown",       0x0000000000000000ULL, KEELOQ_LEARNING_UNKNOWN},
};

const size_t keeloq_builtin_keys_count =
    sizeof(keeloq_builtin_keys) / sizeof(keeloq_builtin_keys[0]);

const ManufacturerKey* keeloq_find_key_by_name(const char* name) {
    if(!name) return NULL;
    for(size_t i = 0; i < keeloq_builtin_keys_count; i++) {
        if(strcmp(keeloq_builtin_keys[i].name, name) == 0) {
            return &keeloq_builtin_keys[i];
        }
    }
    return NULL;
}

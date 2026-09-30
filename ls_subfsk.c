#include "ls_subfsk.h"

#include <string.h>

#define XOSC_HZ 26000000ull

static const uint8_t TEMPLATE[LS_SUBFSK_REGS] = {
    0x02, 0x0D, 0x0B, 0x06, 0x08, 0x32, 0x07, 0x04, 0x14, 0x00, 0x13, 0x02,
    0x12, 0x04, 0x11, 0x83, 0x10, 0x67, 0x15, 0x34, 0x18, 0x18, 0x19, 0x16,
    0x1D, 0x91, 0x1C, 0x00, 0x1B, 0x07, 0x20, 0xFB, 0x22, 0x10, 0x21, 0x56,
    0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static uint64_t absdiff(uint64_t a, uint64_t b) {
    return a > b ? a - b : b - a;
}

/* f_dev = XOSC / 2^17 * (8 + M) * 2^E; compared scaled by 2^17. */
uint8_t ls_cc1101_deviatn(uint32_t dev_hz) {
    if(dev_hz < 600) return 0x34;
    const uint64_t want = (uint64_t)dev_hz << 17;
    uint8_t best = 0x34;
    uint64_t best_err = UINT64_MAX;
    for(int e = 0; e < 8; e++) {
        for(int m = 0; m < 8; m++) {
            uint64_t err = absdiff((XOSC_HZ * (uint64_t)(8 + m)) << e, want);
            if(err < best_err) {
                best_err = err;
                best = (uint8_t)((e << 4) | m);
            }
        }
    }
    return best;
}

/* The RAW path samples GDO0, so DRATE is set to twice the bit rate.
   rate = (256 + M) * 2^E * XOSC / 2^28; compared scaled by 2^28. */
void ls_cc1101_drate(uint32_t bitrate, uint8_t* drate_m, uint8_t* drate_e) {
    if(bitrate < 600) bitrate = 2400;
    if(bitrate > 400000) bitrate = 400000;
    const uint64_t want = (uint64_t)bitrate * 2u << 28;
    uint64_t best_err = UINT64_MAX;
    *drate_m = 0x83;
    *drate_e = 7;
    for(int e = 0; e < 16; e++) {
        for(int m = 0; m < 256; m++) {
            uint64_t err = absdiff(((uint64_t)(256 + m) << e) * XOSC_HZ, want);
            if(err < best_err) {
                best_err = err;
                *drate_m = (uint8_t)m;
                *drate_e = (uint8_t)e;
            }
        }
    }
}

void ls_subfsk_regs(uint8_t* out, uint32_t dev_hz, uint32_t bitrate) {
    uint8_t m, e;
    memcpy(out, TEMPLATE, LS_SUBFSK_REGS);
    ls_cc1101_drate(bitrate, &m, &e);
    out[15] = m;
    out[17] = (uint8_t)(0x60 | e);
    out[19] = ls_cc1101_deviatn(dev_hz);
}

bool ls_subfsk_selftest(void) {
    uint8_t m, e, r[LS_SUBFSK_REGS];
    ls_cc1101_drate(2400, &m, &e);
    if(m != 0x83 || (0x60 | e) != 0x67) return false;
    ls_subfsk_regs(r, 0, 0);
    return ls_cc1101_deviatn(19043) == 0x34 && ls_cc1101_deviatn(9521) == 0x24 &&
           ls_cc1101_deviatn(0) == 0x34 && !memcmp(r, TEMPLATE, LS_SUBFSK_REGS);
}

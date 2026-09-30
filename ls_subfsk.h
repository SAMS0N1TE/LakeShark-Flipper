#pragma once
#include <stdbool.h>
#include <stdint.h>

/* CC1101 register pairs for a 2-FSK async RAW .sub, the trap preset with
   DEVIATN and DRATE filled in from the capture. */
#define LS_SUBFSK_REGS 46
#define LS_SUBFSK_PRESET_HEAD           \
    "Preset: FuriHalSubGhzPresetCustom\n" \
    "Custom_preset_module: CC1101\n"      \
    "Custom_preset_data:"

uint8_t ls_cc1101_deviatn(uint32_t dev_hz);
void ls_cc1101_drate(uint32_t bitrate, uint8_t* drate_m, uint8_t* drate_e);
void ls_subfsk_regs(uint8_t* out, uint32_t dev_hz, uint32_t bitrate);
bool ls_subfsk_selftest(void);

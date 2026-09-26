#include "ls_beacon.h"

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <toolbox/level_duration.h>

/* The carrier is fed to the radio in 10 ms pieces: the async transmitter
   takes one level and duration at a time from interrupt context. */
#define CHUNK_US 10000u

/* The CC1101's PA table value for about -10 dBm differs by band (TI DN013). */
static const struct {
    uint32_t hz;
    const char* name;
    uint8_t pa;
} BANDS[LsBeaconBandCount] = {
    [LsBeaconBand915] = {915000000UL, "915.00", 0x27},
    [LsBeaconBand433] = {433920000UL, "433.92", 0x34},
};

struct LsBeacon {
    const SubGhzDevice* dev;
    LsBeaconBand band;
    bool running;
    uint32_t started;
    const char* error;
    /* Touched from the transmitter's interrupt only while running. */
    volatile uint32_t chunk;
    volatile uint32_t bursts;
    /* The stock OOK preset's registers with a low-power PA table. */
    uint8_t preset[128];
};

static LevelDuration beacon_level(void* ctx) {
    LsBeacon* b = ctx;
    const uint32_t on = LS_BEACON_ON_MS * 1000u / CHUNK_US;
    const uint32_t off = LS_BEACON_OFF_MS * 1000u / CHUNK_US;
    const uint32_t i = b->chunk;
    b->chunk = (i + 1) % (on + off);
    if(i == 0) b->bursts++;
    return level_duration_make(i < on, CHUNK_US);
}

/* Register pairs up to their 0,0 end, then the PA table: index 0 is the
   level for a space, index 1 for a mark (OOK). False if it would not fit. */
static bool build_preset(LsBeacon* b) {
    const uint8_t* regs = subghz_device_cc1101_preset_ook_650khz_async_regs;
    size_t n = 0;
    while(regs[n] || regs[n + 1]) {
        n += 2;
        if(n + 2 + 8 > sizeof(b->preset)) return false;
    }
    memcpy(b->preset, regs, n);
    b->preset[n++] = 0;
    b->preset[n++] = 0;
    const uint8_t pa[8] = {0x00, BANDS[b->band].pa, 0, 0, 0, 0, 0, 0};
    memcpy(b->preset + n, pa, sizeof(pa));
    return true;
}

LsBeacon* ls_beacon_alloc(void) {
    LsBeacon* b = malloc(sizeof(LsBeacon));
    memset(b, 0, sizeof(LsBeacon));
    b->band = LsBeaconBand915;
    return b;
}

void ls_beacon_free(LsBeacon* b) {
    if(!b) return;
    ls_beacon_stop(b);
    free(b);
}

void ls_beacon_set_band(LsBeacon* b, LsBeaconBand band) {
    if(!b->running && band < LsBeaconBandCount) {
        b->band = band;
        b->error = NULL;
    }
}

LsBeaconBand ls_beacon_band(const LsBeacon* b) {
    return b->band;
}

uint32_t ls_beacon_hz(const LsBeacon* b) {
    return BANDS[b->band].hz;
}

const char* ls_beacon_band_name(const LsBeacon* b) {
    return BANDS[b->band].name;
}

bool ls_beacon_start(LsBeacon* b) {
    if(b->running) return true;
    b->error = NULL;
    const uint32_t hz = BANDS[b->band].hz;
    if(!build_preset(b)) {
        b->error = "Radio preset too large";
        return false;
    }
    subghz_devices_init();
    b->dev = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!b->dev) {
        b->error = "No internal Sub-GHz radio";
        subghz_devices_deinit();
        return false;
    }
    /* The internal radio has no begin hook, so this answers false for it
       whatever its state; the stock Sub-GHz app ignores the answer too. */
    subghz_devices_begin(b->dev);
    if(!subghz_devices_is_frequency_valid(b->dev, hz)) {
        b->error = "Band not supported here";
        goto fail;
    }
    subghz_devices_reset(b->dev);
    subghz_devices_load_preset(b->dev, FuriHalSubGhzPresetCustom, b->preset);
    subghz_devices_set_frequency(b->dev, hz);
    b->chunk = 0;
    b->bursts = 0;
    /* The charger's switching is audible to the radio; the stock Sub-GHz
       app holds it off while it transmits, and so does this. */
    furi_hal_power_suppress_charge_enter();
    if(!subghz_devices_start_async_tx(b->dev, beacon_level, b)) {
        furi_hal_power_suppress_charge_exit();
        b->error = "TX refused: region or radio";
        goto fail;
    }
    b->running = true;
    b->started = furi_get_tick();
    return true;

fail:
    subghz_devices_idle(b->dev);
    subghz_devices_end(b->dev);
    subghz_devices_deinit();
    b->dev = NULL;
    return false;
}

void ls_beacon_stop(LsBeacon* b) {
    if(!b->running) return;
    subghz_devices_stop_async_tx(b->dev);
    furi_hal_power_suppress_charge_exit();
    subghz_devices_idle(b->dev);
    subghz_devices_end(b->dev);
    subghz_devices_deinit();
    b->dev = NULL;
    b->running = false;
}

void ls_beacon_tick(LsBeacon* b) {
    if(b->running && ls_beacon_seconds_left(b) == 0) ls_beacon_stop(b);
}

bool ls_beacon_running(const LsBeacon* b) {
    return b->running;
}

uint32_t ls_beacon_seconds_left(const LsBeacon* b) {
    if(!b->running) return 0;
    const uint32_t elapsed = (furi_get_tick() - b->started) / furi_ms_to_ticks(1000);
    return elapsed >= LS_BEACON_RUN_S ? 0 : LS_BEACON_RUN_S - elapsed;
}

uint32_t ls_beacon_bursts(const LsBeacon* b) {
    return b->bursts;
}

const char* ls_beacon_error(const LsBeacon* b) {
    return b->error;
}

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct {
    int index;
    int phase;
    int edges;
    uint32_t span_us;
    uint32_t freq_hz;
    int mod; /* 0 OOK, 1 FSK */
    uint32_t dev_hz;
    uint32_t bitrate;
} LsRecLoadAck;

/* "<key>=<digits>" at *p, no sign, no wider than 32 bits. Hand-rolled because
 * the Flipper's scanf has no %llu: sscanf matched nothing there, every
 * acknowledgement was dropped and every saved-file download timed out. */
static inline bool ls_rec_load_ack_field(const char** p, const char* key, uint32_t* out) {
    size_t n = strlen(key);
    if(strncmp(*p, key, n) != 0) return false;
    const char* s = *p + n;
    uint64_t v = 0;
    int digits = 0;
    while(*s >= '0' && *s <= '9') {
        v = v * 10u + (uint64_t)(*s - '0');
        if(v > UINT32_MAX) return false;
        s++;
        digits++;
    }
    if(!digits) return false;
    *out = (uint32_t)v;
    *p = s;
    return true;
}

/* Optional trailing " <key><digits>" anywhere after p; absent leaves *out. */
static inline void ls_rec_load_ack_opt(const char* p, const char* key, uint32_t* out) {
    const char* at = strstr(p, key);
    if(at) ls_rec_load_ack_field(&at, key, out);
}

/* Only a completed REC LOAD acknowledgement can start a file transfer.
 * Uncorrelated status replies and cached DONE telemetry are not acknowledgements. */
static inline bool ls_rec_load_ack_parse(const char* line, LsRecLoadAck* out) {
    uint32_t index, phase, edges, span, freq;
    const char* p = line;
    if(!line || !out || !ls_rec_load_ack_field(&p, "+OK load=", &index) ||
       !ls_rec_load_ack_field(&p, " ph=", &phase) ||
       !ls_rec_load_ack_field(&p, " e=", &edges) ||
       !ls_rec_load_ack_field(&p, " sp=", &span) ||
       !ls_rec_load_ack_field(&p, " f=", &freq) || index > INT_MAX ||
       phase > 3 || edges > 4096 || freq == 0) return false;
    uint32_t mod = 0, dev = 0, br = 0;
    ls_rec_load_ack_opt(p, " mo=", &mod);
    ls_rec_load_ack_opt(p, " dv=", &dev);
    ls_rec_load_ack_opt(p, " br=", &br);
    *out = (LsRecLoadAck){
        (int)index, (int)phase, (int)edges, span, freq, mod ? 1 : 0, dev, br};
    return true;
}

static inline bool ls_rec_load_ack_ready(
    const LsRecLoadAck* ack, uint32_t sequence, uint32_t before, int index) {
    return sequence != before && ack->index == index && ack->phase == 3 && ack->edges > 0;
}

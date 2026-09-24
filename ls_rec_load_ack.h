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
    *out = (LsRecLoadAck){(int)index, (int)phase, (int)edges, (uint32_t)span, (uint32_t)freq};
    return true;
}

static inline bool ls_rec_load_ack_ready(
    const LsRecLoadAck* ack, uint32_t sequence, uint32_t before, int index) {
    return sequence != before && ack->index == index && ack->phase == 3 && ack->edges > 0;
}

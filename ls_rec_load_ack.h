#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

typedef struct {
    int index;
    int phase;
    int edges;
    uint32_t span_us;
    uint32_t freq_hz;
} LsRecLoadAck;

/* Only a completed REC LOAD acknowledgement can start a file transfer.
 * Uncorrelated status replies and cached DONE telemetry are not acknowledgements. */
static inline bool ls_rec_load_ack_parse(const char* line, LsRecLoadAck* out) {
    unsigned long long index, phase, edges, span, freq;
    if(!line || !out || sscanf(line, "+OK load=%llu ph=%llu e=%llu sp=%llu f=%llu",
       &index, &phase, &edges, &span, &freq) != 5 || index > INT_MAX ||
       phase > 3 || edges > 4096 ||
       span > UINT32_MAX || freq == 0 || freq > UINT32_MAX) return false;
    *out = (LsRecLoadAck){(int)index, (int)phase, (int)edges, (uint32_t)span, (uint32_t)freq};
    return true;
}

static inline bool ls_rec_load_ack_ready(
    const LsRecLoadAck* ack, uint32_t sequence, uint32_t before, int index) {
    return sequence != before && ack->index == index && ack->phase == 3 && ack->edges > 0;
}

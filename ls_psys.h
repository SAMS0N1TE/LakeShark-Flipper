#pragma once
/* The radio's P25 system state and its card profiles, as the head sees them.

   +OK psys p2f=1 p2g=3 pf=p25_profile_seabrook.txt sys=Seabrook_Station geo=off nu=4 unac=2E7
   %P <index> <total> <file> <system>

   strtoul and hand-rolled matching only: the Flipper's scanf has no %llu,
   and a parser that works on the host and not on the device is the failure
   ls_rec_load_ack.h records. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LS_PSYS_TEXT 48

typedef struct {
    bool p2_follow;
    uint32_t p2_grants;
    char profile[LS_PSYS_TEXT]; /* "" when none is loaded */
    char system[LS_PSYS_TEXT];
    char geo[16]; /* off | paused | gpsoff | nofix | siteN/M */
    uint32_t held; /* valid-looking frames not counted as signal */
    uint16_t held_nac;
} LsPsys;

typedef struct {
    int index;
    int total;
    char name[LS_PSYS_TEXT]; /* "" past the end of the list */
    char system[LS_PSYS_TEXT];
} LsProfRow;

/* Copy the next space-delimited word, turning "-" into "" and "_" back into
   spaces, which is how the radio sanitizes free text for the wire. */
static inline const char* ls_psys_word(const char* p, char* out, size_t cap, bool unsanitize) {
    while(*p == ' ') p++;
    size_t n = 0;
    while(*p && *p != ' ' && *p != '\r' && *p != '\n') {
        if(n + 1 < cap) out[n++] = (unsanitize && *p == '_') ? ' ' : *p;
        p++;
    }
    out[n] = '\0';
    if(!strcmp(out, "-")) out[0] = '\0';
    return p;
}

static inline bool ls_psys_parse(const char* line, LsPsys* out) {
    if(!line || !out || strncmp(line, "+OK psys ", 9) != 0) return false;
    LsPsys s;
    memset(&s, 0, sizeof(s));
    const char* p = line + 9;
    char key[8], val[LS_PSYS_TEXT];
    while(*p) {
        while(*p == ' ') p++;
        const char* eq = strchr(p, '=');
        if(!eq || (size_t)(eq - p) >= sizeof(key)) break;
        memcpy(key, p, (size_t)(eq - p));
        key[eq - p] = '\0';
        p = ls_psys_word(eq + 1, val, sizeof(val), !strcmp(key, "sys"));
        if(!strcmp(key, "p2f"))
            s.p2_follow = val[0] == '1';
        else if(!strcmp(key, "p2g"))
            s.p2_grants = (uint32_t)strtoul(val, NULL, 10);
        else if(!strcmp(key, "pf"))
            strncpy(s.profile, val, sizeof(s.profile) - 1);
        else if(!strcmp(key, "sys"))
            strncpy(s.system, val, sizeof(s.system) - 1);
        else if(!strcmp(key, "geo"))
            strncpy(s.geo, val, sizeof(s.geo) - 1);
        else if(!strcmp(key, "nu"))
            s.held = (uint32_t)strtoul(val, NULL, 10);
        else if(!strcmp(key, "unac"))
            s.held_nac = (uint16_t)strtoul(val, NULL, 16);
    }
    *out = s;
    return true;
}

/* line starts after the '%'. */
static inline bool ls_prof_row_parse(const char* line, LsProfRow* out) {
    if(!line || !out || line[0] != 'P') return false;
    char* end = NULL;
    const char* p = line + 1;
    long index = strtol(p, &end, 10);
    if(end == p) return false;
    p = end;
    long total = strtol(p, &end, 10);
    if(end == p || total < 0) return false;
    LsProfRow r;
    memset(&r, 0, sizeof(r));
    r.index = (int)index;
    r.total = (int)total;
    p = ls_psys_word(end, r.name, sizeof(r.name), false);
    ls_psys_word(p, r.system, sizeof(r.system), true);
    *out = r;
    return true;
}

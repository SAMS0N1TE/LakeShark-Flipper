#pragma once

/*LS-840  What the alert stack needs from its host, and nothing else.

   Ported from ZeroMesh the same way the map was, and for the same reason: it
   is written, it works, and it is exactly the feature a receiver in a pocket
   wants. A scanner you cannot see is a scanner that has to tell you when
   something happened.

   Coupling is five fields - three toggles and the scanned ringtone list - so
   like LsMapCtx this takes a context rather than the app, which keeps the
   door open to using it on the P4 later. */

#include <furi.h>

#define RINGTONE_NAME_MAX   32
#define RINGTONE_CUSTOM_MAX 24

/* Shared with ZeroMesh on purpose. Ringtones are small, the operator has
   already collected some, and asking them to keep two copies of the same
   .rtttl files in two directories would be rude. */
#define RINGTONE_DIR "/ext/apps_data/zeromesh/ringtones"

/* The built-in tones, ahead of anything the operator drops in RINGTONE_DIR.
   ZeroMesh's list, kept whole - they were tuned to be distinguishable through
   a coat pocket, which is a harder problem than it sounds. */
typedef enum {
    RingtoneNone = 0,
    RingtoneShort,
    RingtoneDouble,
    RingtoneTriple,
    RingtoneLong,
    RingtoneSOS,
    RingtoneChirp,
    RingtoneNokia,
    RingtoneDescend,
    RingtoneBounce,
    RingtoneAlert,
    RingtonePulse,
    RingtoneSiren,
    RingtoneBeep3,
    RingtoneTrill,
    RingtoneMario,
    RingtoneLevelUp,
    RingtoneMetric,
    RingtoneMinimalist,
    RINGTONE_COUNT
} RingtoneType;

/* Which event fired, so the alert can differ by kind - a page arriving and a
   capture completing should not sound the same when the device is in a bag. */
typedef enum {
    LsAlertPage,      /* a POCSAG page decoded */
    LsAlertVoice,     /* P25 voice went active */
    LsAlertAircraft,  /* an aircraft appeared that was not being tracked */
    LsAlertCapture,   /* REC caught something */
    LsAlertLink,      /* the link to the radio came up or went away */
    LsAlertCount,
} LsAlertKind;

typedef struct {
    /* Independent, as in ZeroMesh: the useful combination in a pocket is
       vibration on and sound off, and that should not require turning off
       alerts altogether. */
    bool led;
    bool vibro;

    /* NOT a boolean: the selected tone, with RingtoneNone meaning silent, and
       anything at or past RINGTONE_COUNT indexing custom_files. ZeroMesh folds
       the on/off into the selection, which is right - "which sound" and
       "whether there is a sound" are one question. */
    uint16_t ringtone;

    /* Per-kind enable. Being told about every aircraft in a busy corridor is
       worse than being told about none. */
    bool on[LsAlertCount];

    uint16_t custom_count;
    char custom_files[RINGTONE_CUSTOM_MAX][RINGTONE_NAME_MAX];
} LsAlertCtx;

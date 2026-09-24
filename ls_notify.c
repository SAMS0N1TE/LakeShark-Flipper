#include "ls_notify.h"
#include "ls_rtttl.h"
#include <furi_hal_speaker.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

const char* ringtone_names[] = {
    "Off",
    "Short",
    "Double",
    "Triple",
    "Long",
    "SOS",
    "Chirp",
    "Nokia",
    "Descend",
    "Bounce",
    "Alert",
    "Pulse",
    "Siren",
    "Beep3",
    "Trill",
    "Mario",
    "LevelUp",
    "Metric",
    "Minimal",
};

static const NotificationSequence seq_vibro_short = {&message_vibro_on, &message_do_not_reset, NULL};
static const NotificationSequence seq_led_flash = {&message_blue_255, &message_do_not_reset, NULL};
static const NotificationSequence seq_vibro_led = {
    &message_vibro_on, &message_blue_255, &message_do_not_reset, NULL,
};
static const NotificationSequence seq_alert_reset = {
    &message_vibro_off, &message_blue_0, NULL,
};

bool ls_alert_cancelled(const LsAlertCtx* app) {
    return app->generation &&
           atomic_load(app->generation) != app->playback_generation;
}

bool ls_alert_delay(const LsAlertCtx* app, uint32_t milliseconds) {
    while(milliseconds && !ls_alert_cancelled(app)) {
        uint32_t slice = milliseconds > 10 ? 10 : milliseconds;
        furi_delay_ms(slice);
        milliseconds -= slice;
    }
    return !ls_alert_cancelled(app);
}

static void play_nokia(LsAlertCtx* app) {
    const int melody[] = {659, 587, 370, 415, 554, 494, 370, 330};
    const int durations[] = {125, 125, 250, 250, 125, 125, 250, 250};
    for(int i = 0; i < 8; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        if(!ls_alert_delay(app, durations[i])) return;
        furi_hal_speaker_stop();
        if(!ls_alert_delay(app, 20)) return;
    }
}

static void play_descend(LsAlertCtx* app) {
    for(int f = 1200; f >= 600; f -= 100) {
        furi_hal_speaker_start((float)f, 0.6f);
        if(!ls_alert_delay(app, 80)) return;
    }
    furi_hal_speaker_stop();
}

static void play_bounce(LsAlertCtx* app) {
    const int melody[] = {600, 800, 600, 900, 600, 1000};
    for(int i = 0; i < 6; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        if(!ls_alert_delay(app, 100)) return;
        furi_hal_speaker_stop();
        if(!ls_alert_delay(app, 30)) return;
    }
}

static void play_alert(LsAlertCtx* app) {
    for(int i = 0; i < 3; i++) {
        furi_hal_speaker_start(900.0f, 0.7f);
        if(!ls_alert_delay(app, 150)) return;
        furi_hal_speaker_stop();
        if(!ls_alert_delay(app, 100)) return;
        furi_hal_speaker_start(700.0f, 0.7f);
        if(!ls_alert_delay(app, 150)) return;
        furi_hal_speaker_stop();
        if(i < 2) if(!ls_alert_delay(app, 100)) return;
    }
}

static void play_pulse(LsAlertCtx* app) {
    for(int i = 0; i < 4; i++) {
        furi_hal_speaker_start(800.0f, 0.5f + (float)i * 0.1f);
        if(!ls_alert_delay(app, 120)) return;
        furi_hal_speaker_stop();
        if(!ls_alert_delay(app, 80)) return;
    }
}

static void play_siren(LsAlertCtx* app) {
    for(int cycle = 0; cycle < 2; cycle++) {
        for(int f = 600; f <= 900; f += 75) {
            furi_hal_speaker_start((float)f, 0.6f);
            if(!ls_alert_delay(app, 40)) return;
        }
        for(int f = 900; f >= 600; f -= 75) {
            furi_hal_speaker_start((float)f, 0.6f);
            if(!ls_alert_delay(app, 40)) return;
        }
    }
    furi_hal_speaker_stop();
}

static void play_beep3(LsAlertCtx* app) {
    for(int i = 0; i < 3; i++) {
        furi_hal_speaker_start(1000.0f, 0.6f);
        if(!ls_alert_delay(app, 100)) return;
        furi_hal_speaker_stop();
        if(i < 2) if(!ls_alert_delay(app, 100)) return;
    }
}

static void play_trill(LsAlertCtx* app) {
    const int melody[] = {800, 1000, 800, 1000, 800, 1000, 1200};
    const int durations[] = {80, 80, 80, 80, 80, 80, 200};
    for(int i = 0; i < 7; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        if(!ls_alert_delay(app, durations[i])) return;
        furi_hal_speaker_stop();
        if(i < 6) if(!ls_alert_delay(app, 30)) return;
    }
}

static void play_mario(LsAlertCtx* app) {
    const int melody[] = {659, 659, 659, 523, 659, 784};
    const int durations[] = {150, 150, 150, 100, 150, 300};
    for(int i = 0; i < 6; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        if(!ls_alert_delay(app, durations[i])) return;
        furi_hal_speaker_stop();
        if(i < 5) if(!ls_alert_delay(app, 50)) return;
    }
}

static void play_levelup(LsAlertCtx* app) {
    const int melody[] = {523, 659, 784, 1047, 1319, 1568};
    for(int i = 0; i < 6; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        if(!ls_alert_delay(app, 100)) return;
        furi_hal_speaker_stop();
        if(i < 5) if(!ls_alert_delay(app, 30)) return;
    }
}

static void play_metric(LsAlertCtx* app) {
    for(int i = 0; i < 4; i++) {
        furi_hal_speaker_start(1047.0f, 0.6f);
        if(!ls_alert_delay(app, 120)) return;
        furi_hal_speaker_stop();
        if(i < 3) if(!ls_alert_delay(app, 120)) return;
    }
}

static void play_minimalist(LsAlertCtx* app) {
    furi_hal_speaker_start(1047.0f, 0.6f);
    if(!ls_alert_delay(app, 150)) return;
    furi_hal_speaker_stop();
    if(!ls_alert_delay(app, 150)) return;
    furi_hal_speaker_start(1047.0f, 0.6f);
    if(!ls_alert_delay(app, 150)) return;
    furi_hal_speaker_stop();
}

static void play_ringtone_body(LsAlertCtx* app) {
    if(app->ringtone == RingtoneNone) return;


    if(app->ringtone >= RINGTONE_COUNT) {
        rtttl_play_custom(app, app->ringtone);
        return;
    }

    switch(app->ringtone) {
    case RingtoneShort:
        furi_hal_speaker_start(800.0f, 0.6f);
        if(!ls_alert_delay(app, 100)) return;
        furi_hal_speaker_stop();
        break;

    case RingtoneDouble:
        furi_hal_speaker_start(800.0f, 0.6f);
        if(!ls_alert_delay(app, 80)) return;
        furi_hal_speaker_stop();
        if(!ls_alert_delay(app, 60)) return;
        furi_hal_speaker_start(1000.0f, 0.6f);
        if(!ls_alert_delay(app, 80)) return;
        furi_hal_speaker_stop();
        break;

    case RingtoneTriple:
        for(int i = 0; i < 3; i++) {
            furi_hal_speaker_start(800.0f + (float)(i * 200), 0.6f);
            if(!ls_alert_delay(app, 60)) return;
            furi_hal_speaker_stop();
            if(i < 2) if(!ls_alert_delay(app, 40)) return;
        }
        break;

    case RingtoneLong:
        furi_hal_speaker_start(600.0f, 0.5f);
        if(!ls_alert_delay(app, 400)) return;
        furi_hal_speaker_stop();
        break;

    case RingtoneSOS:
        for(int i = 0; i < 3; i++) {
            furi_hal_speaker_start(800.0f, 0.6f);
            if(!ls_alert_delay(app, 50)) return;
            furi_hal_speaker_stop();
            if(!ls_alert_delay(app, 50)) return;
        }
        if(!ls_alert_delay(app, 100)) return;
        for(int i = 0; i < 3; i++) {
            furi_hal_speaker_start(800.0f, 0.6f);
            if(!ls_alert_delay(app, 150)) return;
            furi_hal_speaker_stop();
            if(!ls_alert_delay(app, 50)) return;
        }
        if(!ls_alert_delay(app, 100)) return;
        for(int i = 0; i < 3; i++) {
            furi_hal_speaker_start(800.0f, 0.6f);
            if(!ls_alert_delay(app, 50)) return;
            furi_hal_speaker_stop();
            if(i < 2) if(!ls_alert_delay(app, 50)) return;
        }
        break;

    case RingtoneChirp:
        for(int f = 400; f <= 1200; f += 50) {
            furi_hal_speaker_start((float)f, 0.5f);
            if(!ls_alert_delay(app, 15)) return;
        }
        furi_hal_speaker_stop();
        break;

    case RingtoneNokia:
        play_nokia(app);
        break;

    case RingtoneDescend:
        play_descend(app);
        break;

    case RingtoneBounce:
        play_bounce(app);
        break;

    case RingtoneAlert:
        play_alert(app);
        break;

    case RingtonePulse:
        play_pulse(app);
        break;

    case RingtoneSiren:
        play_siren(app);
        break;

    case RingtoneBeep3:
        play_beep3(app);
        break;

    case RingtoneTrill:
        play_trill(app);
        break;

    case RingtoneMario:
        play_mario(app);
        break;

    case RingtoneLevelUp:
        play_levelup(app);
        break;

    case RingtoneMetric:
        play_metric(app);
        break;

    case RingtoneMinimalist:
        play_minimalist(app);
        break;

    default:
        break;
    }

}

void play_ringtone(LsAlertCtx* app) {
    if(app->ringtone == RingtoneNone || ls_alert_cancelled(app)) return;
    /* Short acquisition avoids delaying mute/exit behind another sound owner. */
    if(!furi_hal_speaker_acquire(10)) return;
    if(!ls_alert_cancelled(app)) play_ringtone_body(app);
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

void notify_rx_message(LsAlertCtx* app) {
    if(ls_alert_cancelled(app)) return;
    if(app->vibro || app->led) {
        NotificationApp* notify = furi_record_open(RECORD_NOTIFICATION);
        if(app->vibro && app->led)
            notification_message_block(notify, &seq_vibro_led);
        else if(app->vibro)
            notification_message_block(notify, &seq_vibro_short);
        else
            notification_message_block(notify, &seq_led_flash);
        ls_alert_delay(app, 100);
        /* Always clear outputs, including cancellation and app shutdown. */
        notification_message_block(notify, &seq_alert_reset);
        furi_record_close(RECORD_NOTIFICATION);
    }
    play_ringtone(app);
}

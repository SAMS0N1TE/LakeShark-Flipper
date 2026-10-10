"""Exercise the production text parser on the host, without ports or Furi."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class LinkTelemetryTest(unittest.TestCase):
    def test_modes_aux_and_interleaved_frames(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        header = (root / "ls_link.h").read_text()
        header = header[header.index("typedef enum {"):header.index("typedef struct LsLink")]
        link = (root / "ls_link.c").read_text()
        parser = link[link.index("static const char* kv("):link.index("static void parse_prof(LsLink* link, char* line);")]
        handler = link[link.index("static void handle_line("):link.index("static void rx_isr(")]
        source = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ls_psys.h"
#include "ls_rec_load_ack.h"
#define FURI_LOG_I(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
static uint32_t tick = 100;
static uint32_t furi_get_tick(void) { return tick; }
#define FuriWaitForever 0
#define furi_mutex_acquire(...) ((void)0)
#define furi_mutex_release(...) ((void)0)
static size_t strlcpy(char *dst, const char *src, size_t cap) {
    size_t n = strlen(src);
    if(cap) { size_t k = n < cap - 1 ? n : cap - 1; memcpy(dst, src, k); dst[k] = 0; }
    return n;
}
""" + header + r"""
typedef struct {
    LsTelemetry parsed, tel;
    bool have_tel;
    uint32_t last_tel_tick, frames, replies, bad, last_reply_tick, error_seq;
    char radio_ver[96], last_reply[64], last_error[64];
    LsPsys psys; uint32_t psys_seq;
    LsRecLoadAck rec_load_ack; uint32_t rec_load_ack_seq;
} LsLink;
""" + parser + r"""
static void parse_rec_file(LsLink* link, char* line) { (void)link; (void)line; }
static void parse_prof(LsLink* link, char* line) { (void)link; (void)line; }
static void parse_rec_chunk(LsLink* link, char* line) { (void)link; (void)line; }
""" + handler + r"""
static void frame(LsLink *link, const char *text, bool aux) {
    char line[640]; strlcpy(line, text, sizeof(line));
    if(aux) parse_eq_line(link, line); else parse_telemetry(link, line);
}
int main(void) {
    LsLink link = {0};
    char hello[] = "+HELLO 3 LakeShark_2.10_board_ESP32-P4";
    handle_line(&link, hello);
    assert(link.tel.protocol_version == 3 && strstr(link.radio_ver, "2.10"));
    char error[] = "-ERR fm";
    handle_line(&link, error);
    char pong[] = "+PONG 3 LakeShark";
    handle_line(&link, pong);
    assert(link.error_seq == 1 && !strcmp(link.last_error, "-ERR fm"));
    assert(!strcmp(link.last_reply, "+PONG 3 LakeShark"));
    const char *names[] = {"listen", "scan", "pocsag", "wfm", "acars", "flex", "unknown", "am", "same", "aprs", "ais"};
    for(int i = 0; i <= 10; i++) {
        char text[64]; snprintf(text, sizeof(text), "md=FM fm=%s", names[i]);
        frame(&link, text, false);
        assert(link.tel.fm_submode == (i == 6 ? -1 : i));
        snprintf(text, sizeof(text), "md=FM fm=%d", i);
        frame(&link, text, false);
        assert(link.tel.fm_submode == (i == 6 ? -1 : i));
    }
    frame(&link, "md=FM fm=garbage", false);
    assert(link.tel.fm_submode == -1);
    frame(&link, "md=FM", false);
    assert(link.tel.fm_submode == -1);
    frame(&link, "av=1 wc=1 wn=8 ws=Lake_House wi=10.0.0.2 sr=1 sm=1 s0=1 s1=2 s2=3 s3=4 s4=5 c0=C1:-42 c1=T22:-67 dn=2 dr=-1 di=-", true);
    assert(link.tel.aux.version == 1 && link.tel.aux.tick == 100);
    assert(!strcmp(link.tel.aux.wifi_ssid, "Lake House"));
    assert(link.tel.aux.counts[4] == 5 && link.tel.aux.nearest_m == -1);
    tick = 300;
    frame(&link, "md=REC f=433420000 rmo=1 rdv=19043 rcf=433420000 rbr=2400 up=1", false);
    assert(link.tel.mode == LsModeRec && link.tel.rec_mod == 1 && link.tel.rec_dev_hz == 19043);
    assert(link.tel.aux.tick == 100 && link.tel.aux.drones == 2);
    frame(&link, "eq=2 eh=300 xon=1 xn=32", true);
    assert(link.tel.aux.tick == 100 && link.tel.aux.sweep_muted == 1);
    assert(link.tel.sweep.on == 1 && link.tel.eq_hp_hz == 300);
    frame(&link, "av=1 wc=0 wn=-1 ws=- wi=- sr=0 sm=0 s0=0 s1=0 s2=0 s3=0 s4=0 c0=- c1=- dn=0 dr=-1 di=-", true);
    assert(link.tel.aux.tick == 300 && link.tel.aux.wifi_ssid[0] == 0);
    frame(&link, "md=ADS-B acn=1 a0=A4E1BF,UAL123,35000,470,271,0,850,42,421234,-711234 rhs=STREAMING board=ESP32-P4", false);
    assert(link.tel.mode == LsModeAdsb && link.tel.ac[0].pos_valid);
    assert(link.tel.ac[0].lat_e4 == 421234 && link.tel.aux.drones == 0);
    frame(&link, "md=ADSB acn=1 a0=A4E1BF,UAL123,35000,470,271,0,850,42", false);
    assert(!link.tel.ac[0].pos_valid);
    char unsupported[] = "-ERR unknown AUX";
    handle_line(&link, unsupported);
    assert(link.tel.aux.version == 0 && link.parsed.aux.version == 0);
    frame(&link, "md=SWEEP", false);
    assert(link.tel.mode == LsModeUnknown);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            c = pathlib.Path(tmp) / "parser.c"
            exe = pathlib.Path(tmp) / "parser.exe"
            c.write_text(source)
            subprocess.run([shutil.which("gcc") or "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root), str(c), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()

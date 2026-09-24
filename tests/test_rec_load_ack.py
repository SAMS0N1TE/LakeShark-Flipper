import pathlib, shutil, subprocess, tempfile, unittest
class LoadAckTest(unittest.TestCase):
    def test_correlated_capture_metadata(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        source = r"""
#include <assert.h>
#include "ls_rec_load_ack.h"
int main(void) {
    LsRecLoadAck a;
    assert(!ls_rec_load_ack_parse("+OK ph=3 e=200 sp=215019 f=434070000", &a));
    assert(ls_rec_load_ack_parse("+OK load=15 ph=3 e=250 sp=290459 f=434070000 th=0", &a));
    assert(a.edges == 250 && a.span_us == 290459 && a.freq_hz == 434070000);
    assert(!ls_rec_load_ack_ready(&a, 7, 7, 15));
    assert(!ls_rec_load_ack_ready(&a, 8, 7, 0));
    assert(ls_rec_load_ack_ready(&a, 8, 7, 15));
    assert(ls_rec_load_ack_parse("+OK load=0 ph=3 e=200 sp=215019 f=433920000", &a));
    assert(ls_rec_load_ack_ready(&a, 9, 8, 0));
    assert(a.edges == 200 && a.freq_hz == 433920000);
    assert(!ls_rec_load_ack_parse("+OK load=-1 ph=3 e=250 sp=1 f=1", &a));
    assert(!ls_rec_load_ack_parse("+OK load=0 ph=3 e=4097 sp=1 f=1", &a));
    assert(!ls_rec_load_ack_parse("+OK load=0 ph=3 e=250 sp=4294967296 f=1", &a));
    assert(!ls_rec_load_ack_parse("+OK load=0 ph=3 e=250 sp=1 f=0", &a));
    assert(ls_rec_load_ack_parse("+OK load=0 ph=2 e=250 sp=1 f=1", &a));
    assert(!ls_rec_load_ack_ready(&a, 10, 9, 0));
    assert(ls_rec_load_ack_parse("+OK load=0 ph=3 e=0 sp=0 f=1", &a));
    assert(!ls_rec_load_ack_ready(&a, 10, 9, 0));
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            c = pathlib.Path(tmp) / "test.c"
            exe = pathlib.Path(tmp) / "test.exe"
            c.write_text(source)
            subprocess.run([shutil.which("gcc") or "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root), str(c), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
if __name__ == "__main__": unittest.main()

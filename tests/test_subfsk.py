import pathlib, shutil, subprocess, tempfile, unittest
class SubFskTest(unittest.TestCase):
    def test_trap_preset(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        source = r"""
#include <assert.h>
#include "ls_subfsk.h"
int main(void) {
    uint8_t m, e, r[LS_SUBFSK_REGS];
    assert(ls_subfsk_selftest());
    assert(ls_cc1101_deviatn(19043) == 0x34 && ls_cc1101_deviatn(9521) == 0x24);
    assert(ls_cc1101_deviatn(0) == 0x34 && ls_cc1101_deviatn(599) == 0x34);
    assert(ls_cc1101_deviatn(47607) == 0x47);
    ls_cc1101_drate(2400, &m, &e);
    assert(m == 0x83 && e == 7);
    ls_cc1101_drate(0, &m, &e);
    assert(m == 0x83 && e == 7);
    ls_cc1101_drate(4800, &m, &e);
    assert(m == 0x83 && e == 8);
    ls_subfsk_regs(r, 9521, 1200);
    assert(r[14] == 0x11 && r[16] == 0x10 && r[17] == 0x66 && r[18] == 0x15 && r[19] == 0x24);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            c = pathlib.Path(tmp) / "test.c"
            exe = pathlib.Path(tmp) / "test.exe"
            c.write_text(source)
            subprocess.run([shutil.which("gcc") or "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root), str(c), str(root / "ls_subfsk.c"), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
if __name__ == "__main__": unittest.main()

import pathlib, shutil, subprocess, tempfile, unittest
class PsysTest(unittest.TestCase):
    def test_status_and_profile_rows(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        source = r"""
#include <assert.h>
#include <string.h>
#include "ls_psys.h"
int main(void) {
    LsPsys s;
    assert(ls_psys_parse("+OK psys p2f=1 p2g=3 pf=p25_profile_seabrook.txt sys=Seabrook_Station geo=site2/4 nu=4 unac=2E7", &s));
    assert(s.p2_follow && s.p2_grants == 3);
    assert(!strcmp(s.profile, "p25_profile_seabrook.txt"));
    assert(!strcmp(s.system, "Seabrook Station"));
    assert(!strcmp(s.geo, "site2/4"));
    assert(s.held == 4 && s.held_nac == 0x2E7);
    assert(ls_psys_parse("+OK psys p2f=0 p2g=0 pf=- sys=- geo=off nu=0 unac=000", &s));
    assert(!s.p2_follow && s.profile[0] == 0 && s.system[0] == 0);
    assert(!ls_psys_parse("+OK md=P25", &s));
    assert(!ls_psys_parse("+OK psy", &s));

    LsProfRow r;
    assert(ls_prof_row_parse("P 1 2 p25_profile_seabrook.txt Seabrook_Station", &r));
    assert(r.index == 1 && r.total == 2);
    assert(!strcmp(r.name, "p25_profile_seabrook.txt"));
    assert(!strcmp(r.system, "Seabrook Station"));
    assert(ls_prof_row_parse("P 0 0 - -", &r));
    assert(r.total == 0 && r.name[0] == 0);
    assert(!ls_prof_row_parse("S 0 1 434070000 12 x", &r));
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

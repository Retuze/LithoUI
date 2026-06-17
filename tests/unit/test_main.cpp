#include <cstdio>

bool run_region_tests();
bool run_dirty_list_tests();
bool run_painter_tests();
bool run_pfb_tests();

int main() {
    printf("LithoUI unit tests\n\n");

    bool ok = true;
    if (!run_region_tests())      ok = false;
    if (!run_painter_tests())     ok = false;
    if (!run_dirty_list_tests())  ok = false;
    if (!run_pfb_tests())         ok = false;
    // if (!run_pfb_tests())         ok = false;

    printf("\n%s\n", ok ? "ALL PASS" : "SOME FAILED");
    return ok ? 0 : 1;
}

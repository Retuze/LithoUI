#pragma once
#include <cstdio>

// Minimal test framework — zero dependencies.
//
// Usage:
//   TEST(dirty_merge) {
//       DirtyList dl;
//       dl.markDirty({0,0,10,10});
//       EXPECT(dl.count() == 1);
//   }
//
//   File exports a run function that calls each test by name:
//   bool run_dirty_list_tests() { ... }

#define TEST(name) static bool test_##name()

#define EXPECT(cond)                                                       \
    do { if (!(cond)) {                                                    \
        fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        return false;                                                      \
    } } while(0)

#define EXPECT_EQ(a, b)    EXPECT((a) == (b))
#define EXPECT_NE(a, b)    EXPECT((a) != (b))
#define EXPECT_TRUE(a)     EXPECT(a)
#define EXPECT_FALSE(a)    EXPECT(!(a))

// Helper: run a named test and update `ok`.
static bool runTest(const char* name, bool (*fn)(), bool& ok) {
    printf("  %-40s", name);
    bool pass = fn();
    printf("%s\n", pass ? "ok" : "FAIL");
    if (!pass) ok = false;
    return pass;
}

// Use in each test file's runner:
//   bool run_xxx_tests() {
//       bool ok = true;
//       runTest("basic", test_basic, ok);
//       runTest("merge", test_merge, ok);
//       return ok;
//   }

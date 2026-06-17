#include "core/region.hpp"
#include "tests/unit/test_runner.hpp"

using namespace litho;

TEST(region_basic) {
    Region r = {0, 0, 100, 50};
    EXPECT_EQ(r.x, 0);
    EXPECT_EQ(r.y, 0);
    EXPECT_EQ(r.width, 100);
    EXPECT_EQ(r.height, 50);
    EXPECT_FALSE(r.isEmpty());
    return true;
}

TEST(region_empty) {
    Region r;
    EXPECT_TRUE(r.isEmpty());
    r = {10, 10, 0, 0};
    EXPECT_TRUE(r.isEmpty());
    r = {10, 10, 50, 0};
    EXPECT_TRUE(r.isEmpty());
    r = {10, 10, 0, 50};
    EXPECT_TRUE(r.isEmpty());
    return true;
}

TEST(region_contains) {
    Region r = {10, 10, 100, 50};
    EXPECT_TRUE(r.contains(10, 10));    // top-left corner
    EXPECT_TRUE(r.contains(109, 59));   // bottom-right (exclusive)
    EXPECT_FALSE(r.contains(110, 10));  // right edge out
    EXPECT_FALSE(r.contains(10, 60));   // bottom edge out
    EXPECT_FALSE(r.contains(0, 0));     // far out
    return true;
}

bool run_region_tests() {
    printf("=== Region ===\n");
    bool ok = true;
    runTest("basic",    test_region_basic,    ok);
    runTest("empty",    test_region_empty,    ok);
    runTest("contains", test_region_contains, ok);
    return ok;
}

#include "core/dirty_list.hpp"
#include "tests/unit/test_runner.hpp"

using namespace litho;

// ---- basic ----

TEST(dirty_add_one) {
    DirtyList dl;
    dl.markDirty({0, 0, 10, 10});
    EXPECT_EQ(dl.count(), 1);
    EXPECT_EQ(dl.regions()[0].x, 0);
    EXPECT_EQ(dl.regions()[0].y, 0);
    EXPECT_EQ(dl.regions()[0].width, 10);
    EXPECT_EQ(dl.regions()[0].height, 10);
    return true;
}

TEST(dirty_empty_is_ignored) {
    DirtyList dl;
    dl.markDirty({0, 0, 0, 0});
    EXPECT_EQ(dl.count(), 0);
    dl.markDirty({5, 5, 10, 0});
    EXPECT_EQ(dl.count(), 0);
    return true;
}

// ---- merge ----

TEST(dirty_merge_overlapping) {
    DirtyList dl;
    dl.markDirty({0, 0, 10, 10});
    dl.markDirty({5, 5, 10, 10});   // overlaps → merged
    EXPECT_EQ(dl.count(), 1);
    EXPECT_EQ(dl.regions()[0].x, 0);
    EXPECT_EQ(dl.regions()[0].y, 0);
    EXPECT_EQ(dl.regions()[0].width, 15);
    EXPECT_EQ(dl.regions()[0].height, 15);
    return true;
}

TEST(dirty_merge_contained) {
    DirtyList dl;
    dl.markDirty({0, 0, 20, 20});
    dl.markDirty({5, 5, 5, 5});     // fully inside → absorbed, no growth
    EXPECT_EQ(dl.count(), 1);
    EXPECT_EQ(dl.regions()[0].width, 20);
    EXPECT_EQ(dl.regions()[0].height, 20);
    return true;
}

TEST(dirty_merge_adjacent) {
    DirtyList dl;
    dl.markDirty({0, 0, 10, 10});
    dl.markDirty({10, 0, 10, 10});  // right edge touches → overlaps at edge
    EXPECT_EQ(dl.count(), 1);
    EXPECT_EQ(dl.regions()[0].x, 0);
    EXPECT_EQ(dl.regions()[0].width, 20);
    return true;
}

TEST(dirty_no_merge_disjoint) {
    DirtyList dl;
    dl.markDirty({0, 0, 10, 10});
    dl.markDirty({50, 50, 10, 10}); // far away
    EXPECT_EQ(dl.count(), 2);
    return true;
}

TEST(dirty_chain_merge) {
    DirtyList dl;
    dl.markDirty({0, 0, 10, 10});    // A
    dl.markDirty({40, 0, 10, 10});   // B — disjoint from A
    EXPECT_EQ(dl.count(), 2);
    dl.markDirty({5, 0, 40, 10});    // C — overlaps A and B, merges all three
    EXPECT_EQ(dl.count(), 1);
    EXPECT_EQ(dl.regions()[0].x, 0);
    EXPECT_EQ(dl.regions()[0].width, 50);
    return true;
}

// ---- overflow ----

TEST(dirty_overflow_collapse) {
    DirtyList dl;
    // Fill with 16 disjoint rects
    for (int i = 0; i < 16; i++) {
        dl.markDirty({(int16_t)(i * 30), 0, 20, 20});
    }
    EXPECT_EQ(dl.count(), 16);
    // 17th triggers collapse
    dl.markDirty({0, 50, 10, 10});
    EXPECT_EQ(dl.count(), 1);
    return true;
}

// ---- edge cases ----

TEST(dirty_negative_clip) {
    DirtyList dl;
    dl.markDirty({-10, -10, 20, 20});
    EXPECT_EQ(dl.count(), 1);
    EXPECT_EQ(dl.regions()[0].x, -10);
    return true;
}

TEST(dirty_clear) {
    DirtyList dl;
    dl.markDirty({0, 0, 10, 10});
    dl.clear();
    EXPECT_EQ(dl.count(), 0);
    dl.markDirty({5, 5, 10, 10});
    EXPECT_EQ(dl.count(), 1);
    return true;
}

// ---- runner ----

bool run_dirty_list_tests() {
    printf("=== DirtyList ===\n");
    bool ok = true;
    runTest("add_one",           test_dirty_add_one,           ok);
    runTest("empty_ignored",     test_dirty_empty_is_ignored,   ok);
    runTest("merge_overlap",     test_dirty_merge_overlapping,  ok);
    runTest("merge_contained",   test_dirty_merge_contained,    ok);
    runTest("merge_adjacent",    test_dirty_merge_adjacent,     ok);
    runTest("no_merge_disjoint", test_dirty_no_merge_disjoint,  ok);
    runTest("chain_merge",       test_dirty_chain_merge,        ok);
    runTest("overflow_collapse", test_dirty_overflow_collapse,  ok);
    runTest("negative_clip",     test_dirty_negative_clip,      ok);
    runTest("clear",             test_dirty_clear,              ok);
    return ok;
}

#include <unity.h>
#include "sd_policy.h"

void test_no_flush_when_empty()
{
    SdFlushPolicy p{2000, 1800};
    SdFlushState st{1000};
    TEST_ASSERT_FALSE(should_flush(p, st, 0, 1500));
}

void test_flush_on_watermark()
{
    SdFlushPolicy p{2000, 1800};
    SdFlushState st{1000};
    TEST_ASSERT_TRUE(should_flush(p, st, 1800, 1100));
    TEST_ASSERT_TRUE(should_flush(p, st, 2000, 1100));
}

void test_flush_on_time()
{
    SdFlushPolicy p{2000, 1800};
    SdFlushState st{1000};
    TEST_ASSERT_FALSE(should_flush(p, st, 100, 2500)); // 1500ms elapsed
    TEST_ASSERT_TRUE(should_flush(p, st, 100, 3000));  // 2000ms elapsed
}

void test_wraparound_safe()
{
    // unsigned wrap-around test
    SdFlushPolicy p{2000, 1800};
    SdFlushState st{0xFFFFFF00u};
    uint32_t now = 0x00000100u; // wrapped by 0x200
    // elapsed = 0x200 = 512ms
    TEST_ASSERT_FALSE(should_flush(p, st, 100, now));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_no_flush_when_empty);
    RUN_TEST(test_flush_on_watermark);
    RUN_TEST(test_flush_on_time);
    RUN_TEST(test_wraparound_safe);
    return UNITY_END();
}
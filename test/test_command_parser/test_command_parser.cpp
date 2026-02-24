#include <unity.h>
#include "command_parser.h"

void test_status() {
    CommandEvent ev{};
    TEST_ASSERT_TRUE(parse_command_line("status", &ev));
    TEST_ASSERT_EQUAL((int)CommandType::Status, (int)ev.type);
}

void test_period_ok() {
    CommandEvent ev{};
    TEST_ASSERT_TRUE(parse_command_line("period 4000", &ev));
    TEST_ASSERT_EQUAL((int)CommandType::SetPeriod, (int)ev.type);
    TEST_ASSERT_EQUAL_UINT32(4000, ev.value);
}

void test_period_bad_chars() {
    CommandEvent ev{};
    TEST_ASSERT_FALSE(parse_command_line("period 12x", &ev));
}

void test_pause_on() {
    CommandEvent ev{};
    TEST_ASSERT_TRUE(parse_command_line("pause on", &ev));
    TEST_ASSERT_EQUAL((int)CommandType::PauseOn, (int)ev.type);
}

void test_unknown() {
    CommandEvent ev{};
    TEST_ASSERT_FALSE(parse_command_line("random 123", &ev));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_status);
    RUN_TEST(test_period_ok);
    RUN_TEST(test_period_bad_chars);
    RUN_TEST(test_pause_on);
    RUN_TEST(test_unknown);
    return UNITY_END();
}

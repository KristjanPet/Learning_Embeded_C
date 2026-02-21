#include <unity.h>
#include "button_logic.h"

static void feed(ButtonLogic& b, bool pressed, uint32_t t0, uint32_t t1, uint32_t step_ms)
{
    for (uint32_t t = t0; t <= t1; t += step_ms) {
        (void)b.update(pressed, t);
    }
}

void test_short_press_clean()
{
    ButtonLogic b({50, 800});

    // press at t=0, hold 300ms, release
    feed(b, true, 0, 300, 10);
    // release and keep released
    ButtonEvent ev = ButtonEvent::None;
    for (uint32_t t = 310; t <= 500; t += 10) {
        ev = b.update(false, t);
        if (ev != ButtonEvent::None) break;
    }

    TEST_ASSERT_EQUAL((int)ButtonEvent::ShortPress, (int)ev);
}

void test_long_press()
{
    ButtonLogic b({50, 800});

    // press and hold 1000ms
    ButtonEvent ev = ButtonEvent::None;
    for (uint32_t t = 0; t <= 1000; t += 10) {
        ev = b.update(true, t);
        if (ev == ButtonEvent::LongPress) break;
    }
    TEST_ASSERT_EQUAL((int)ButtonEvent::LongPress, (int)ev);

    // release should not produce short press after long
    ev = ButtonEvent::None;
    for (uint32_t t = 1010; t <= 1200; t += 10) {
        ev = b.update(false, t);
        if (ev != ButtonEvent::None) break;
    }
    TEST_ASSERT_EQUAL((int)ButtonEvent::None, (int)ev);
}

void test_bounce_doesnt_trigger()
{
    ButtonLogic b({50, 800});

    // simulate bounce around press: toggling within debounce window
    (void)b.update(true, 0);
    (void)b.update(false, 10);
    (void)b.update(true, 20);
    (void)b.update(false, 30);
    (void)b.update(true, 40);

    // still within 50ms debounce, no stable press yet
    TEST_ASSERT_EQUAL((int)ButtonEvent::None, (int)b.update(true, 45));

    // after debounce time passes with stable true, still no event
    TEST_ASSERT_EQUAL((int)ButtonEvent::None, (int)b.update(true, 120));

    // release after 200ms hold -> should be short
    ButtonEvent ev = ButtonEvent::None;
    for (uint32_t t = 130; t <= 400; t += 10) {
        ev = b.update(false, t);
        if (ev != ButtonEvent::None) break;
    }
    TEST_ASSERT_EQUAL((int)ButtonEvent::ShortPress, (int)ev);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_short_press_clean);
    RUN_TEST(test_long_press);
    RUN_TEST(test_bounce_doesnt_trigger);
    return UNITY_END();
}

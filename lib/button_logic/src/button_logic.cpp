#include "button_logic.h"

ButtonLogic::ButtonLogic(buttonLogicConfig cfg) : cfg(cfg) {}

void ButtonLogic::reset() {
    stable_pressed = false;
    last_raw = false;
    last_change_ms = 0;
    press_start_ms = 0;
    long_sent = false;
}

ButtonEvent ButtonLogic::update(bool raw_pressed, uint32_t now_ms){
    // detect raw changes (bounce)
    if (raw_pressed != last_raw) {
        last_raw = raw_pressed;
        last_change_ms = now_ms;
    }

    // debounce: accept new stable state only after it stays unchanged for debounce_ms
    if ((now_ms - last_change_ms) >= cfg.debounce_ms) {
        if (stable_pressed != last_raw) {
            stable_pressed = last_raw;

            if (stable_pressed) {
                // stable press started
                press_start_ms = now_ms;
                long_sent = false;
            } else {
                // stable release
                if (!long_sent) {
                    // released before longpress → short press
                    return ButtonEvent::ShortPress;
                }
                // if long already sent, do nothing on release
            }
        }
    }

    // long press event when held long enough (only once)
    if (stable_pressed && !long_sent) {
        if ((now_ms - press_start_ms) >= cfg.longpress_ms) {
            long_sent = true;
            return ButtonEvent::LongPress;
        }
    }

    return ButtonEvent::None;
}
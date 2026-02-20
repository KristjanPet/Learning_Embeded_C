#include "button_logic.h"

ButtonLogic::ButtonLogic(ButtonLogicConfig cfg) : cfg_(cfg) {}

void ButtonLogic::reset() {
    stable_pressed_ = false;
    last_raw_ = false;
    last_change_ms_ = 0;
    press_start_ms_ = 0;
    long_sent_ = false;
}

ButtonEvent ButtonLogic::update(bool raw_pressed, uint32_t now_ms){
    // detect raw changes (bounce)
    if (raw_pressed != last_raw_) {
        last_raw_ = raw_pressed;
        last_change_ms_ = now_ms;
    }

    // debounce: accept new stable state only after it stays unchanged for debounce_ms
    if ((now_ms - last_change_ms_) >= cfg_.debounce_ms) {
        if (stable_pressed_ != last_raw_) {
            stable_pressed_ = last_raw_;

            if (stable_pressed_) {
                // stable press started
                press_start_ms_ = now_ms;
                long_sent_ = false;
            } else {
                // stable release
                if (!long_sent_) {
                    // released before longpress → short press
                    return ButtonEvent::ShortPress;
                }
                // if long already sent, do nothing on release
            }
        }
    }

    // long press event when held long enough (only once)
    if (stable_pressed_ && !long_sent_) {
        if ((now_ms - press_start_ms_) >= cfg_.longpress_ms) {
            long_sent_ = true;
            return ButtonEvent::LongPress;
        }
    }

    return ButtonEvent::None;
}
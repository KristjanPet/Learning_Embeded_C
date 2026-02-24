#pragma once
#include <cstdint>

enum class ButtonEvent : uint8_t { None, ShortPress, LongPress};

struct buttonLogicConfig{
    uint32_t debounce_ms;
    uint32_t longpress_ms;
};

class ButtonLogic{
public:
    explicit ButtonLogic(buttonLogicConfig cfg);

    ButtonEvent update(bool pressed, uint32_t now_ms);

    void reset();

private:
    buttonLogicConfig cfg;

    bool stable_pressed = false;
    bool last_raw = false;
    uint32_t last_change_ms = 0;

    uint32_t press_start_ms = 0;
    bool long_sent = false;
};
#pragma once
#include <cstdint>

struct Sample {
    int count;
    uint32_t timestamp_ms;
};

struct LogEvent{
    LogType type;
    int count;
    uint32_t timestamp_ms;
};

enum class LogType : uint8_t { SENT, DROPPED, RECEIVED, ERROR, STOP, CHANGED };

enum class ButtonEvent : uint8_t { ShortPress, LongPress };
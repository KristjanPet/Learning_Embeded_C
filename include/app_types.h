#pragma once
#include <cstdint>

enum class LogType : uint8_t { SENT, DROPPED, RECEIVED, ERROR, STOP, CHANGED, PAUSED };

enum class ButtonEvent : uint8_t { ShortPress, LongPress };

enum class CommandEvent : uint8_t { TogglePeriod, TogglePause, Status };

struct Sample {
    int count;
    uint32_t timestamp_ms;
};

struct LogEvent{
    LogType type;
    int count;
    uint32_t timestamp_ms;
};
#pragma once
#include <cstdint>

enum class LogType : uint8_t { SENT, DROPPED, RECEIVED, ERROR, STOP, CHANGED, PAUSED };

enum class ButtonEvent : uint8_t { ShortPress, LongPress };

enum class CommandType : uint8_t { SetPeriod, PauseOn, PauseOff, PauseToggle, Status };

struct Sample {
    int count;
    uint32_t timestamp_ms;
};

struct LogEvent{
    LogType type;
    int count;
    uint32_t timestamp_ms;
};

struct CommandEvent {
    CommandType type;
    uint32_t value;   // used for SetPeriod, otherwise 0
};
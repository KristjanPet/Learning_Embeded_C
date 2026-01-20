#pragma once
#include <cstdint>

struct Sample {
    int count;
    uint32_t timestamp_ms;
};

enum class LogType : uint8_t { SENT, DROPPED, RECEIVED, ERROR };

struct LogEvent{
    LogType type;
    int count;
    uint32_t timestamp_ms;
};
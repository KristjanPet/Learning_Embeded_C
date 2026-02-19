#include "command_parser.h"
#include <cstring>
#include <cstdlib>

static bool starts_with(const char* s, const char* pref){
    while (*pref) { if (*s++ != *pref++) return false; }
    return true;
}

bool parse_command_line(const char* line, CommandEvent* out)
{
    if (!line || !out) return false;

    // status
    if (!strcmp(line, "status")) {
        out->type = CommandType::Status;
        out->value = 0;
        return true;
    }

    // help (let UART handle printing help; parser can still recognize it if you want)
    if (!strcmp(line, "help")) {
        // You can either treat this as a command event or let uart() handle it separately.
        return false;
    }

    // pause
    if (!strcmp(line, "pause toggle")) {
        out->type = CommandType::PauseToggle;
        out->value = 0;
        return true;
    }
    if (!strcmp(line, "pause on")) {
        out->type = CommandType::PauseOn;
        out->value = 0;
        return true;
    }
    if (!strcmp(line, "pause off")) {
        out->type = CommandType::PauseOff;
        out->value = 0;
        return true;
    }

    // period N
    if (starts_with(line, "period ")) {
        char* end = nullptr;
        unsigned long v = strtoul(line + 7, &end, 10);

        // must consume at least 1 digit and end exactly at '\0'
        if (end == (line + 7) || *end != '\0') return false;

        // range check
        if (v < 50 || v > 10000) return false;

        out->type = CommandType::SetPeriod;
        out->value = (uint32_t)v;
        return true;
    }

    return false;
}

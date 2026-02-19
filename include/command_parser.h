#pragma once
#include <cstddef>
#include <cstdint>
#include <stdbool.h>
#include "app_types.h"   // your CommandType + CommandEvent

// Parses already-trimmed line (no \r\n), like "period 4000".
// Returns true if recognized and fills out 'out'.
// Returns false if unknown/invalid.
bool parse_command_line(const char* line, CommandEvent* out);

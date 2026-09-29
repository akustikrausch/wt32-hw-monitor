#ifndef PARSER_H
#define PARSER_H

#include "display.h"

// Result of parsing one line from the PC
enum MsgType {
    MSG_INVALID = 0,   // not valid JSON (corrupted or truncated line)
    MSG_DATA,          // full hardware data set
    MSG_HEARTBEAT,     // PC script alive, but no fresh data from LibreHardwareMonitor
};

// Read a line from Serial buffer. Returns true if a complete line was received.
// Lines longer than maxLen are dropped completely instead of being truncated.
bool serial_readLine(char *buf, int maxLen);

// Parse a JSON line. Time sync fields are applied for both data and heartbeat messages.
MsgType parseMessage(const char *json, HWData &data);

#endif

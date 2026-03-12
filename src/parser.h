#ifndef PARSER_H
#define PARSER_H

#include "display.h"

// Read a line from Serial buffer. Returns true if a complete line was received.
bool serial_readLine(char *buf, int maxLen);

// Parse JSON line into HWData struct. Returns true on success.
bool parseHWData(const char *json, HWData &data);

#endif

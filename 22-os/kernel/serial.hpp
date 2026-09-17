#pragma once

#include <stdint.h>

extern "C" void serial_init();
extern "C" void serial_write(const char* text);
extern "C" void serial_write_char(char c);
extern "C" void serial_write_decimal(uint32_t value);

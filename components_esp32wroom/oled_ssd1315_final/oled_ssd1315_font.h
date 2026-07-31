#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool oled_ssd1315_font_ascii(char ch, uint8_t columns[5]);
bool oled_ssd1315_font_chinese(const char *utf8, size_t len, uint16_t rows[16]);

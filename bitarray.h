#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef int32_t bitarray[128]; // 128 * 4 = 4096

void set_bit(bitarray ba, int32_t k);
void clear_bit(bitarray ba, int32_t k);
bool test_bit(bitarray ba, int32_t k);
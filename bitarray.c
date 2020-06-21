// borrowed heavily from: http://www.mathcs.emory.edu/~cheung/Courses/255/Syllabus/1-C-intro/bit-array.html
#include "bitarray.h"

void set_bit(bitarray ba, int32_t k) {
    int32_t i = k/32;
    int32_t pos = k%32;

    uint32_t flag = 1;

    flag <<= pos;
    ba[i] |= flag; 
}

void clear_bit(bitarray ba, int32_t k) {
    int32_t i = k/32;
    int32_t pos = k%32;

    uint32_t flag = 1;

    flag <<= pos;
    flag = ~flag;
    ba[i] &= flag;
}

bool test_bit(bitarray ba, int32_t k) {
    int32_t i = k/32;
    int32_t pos = k%32;

    uint32_t flag = 1;
    flag <<= pos;

    if (ba[i] & flag)
        return true;
    else
        return false;
}

// #include <stdio.h>
// int main() {
//     bitarray b = {0};
//     set_bit(b, 3000);
//     printf("[0] = %d\n", test_bit(b, 0));
//     printf("[1000] = %d\n", test_bit(b, 1000));
//     printf("[2999] = %d\n", test_bit(b, 2999));
//     printf("[3000] = %d\n", test_bit(b, 3000));
//     clear_bit(b, 3000);
//     printf("[3000] after clear = %d\n", test_bit(b, 3000));
// }
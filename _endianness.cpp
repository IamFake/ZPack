//
// Created by neos on 12/15/17.
//

#include "_endianness.h"
#include <cstdint>

Endianness endiannessDetect() {
    volatile int32_t test = 0x01020304;
    volatile char *ptr;
    ptr = (char *) &test;

    return ptr[0] == 0x04 ?
           Endianness::LITTLE :
           (ptr[0] == 0x01 ?
            Endianness::BIG : Endianness::NONE);
}

Endianness endianess = endiannessDetect();

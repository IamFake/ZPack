#ifndef PACKER_ENDIENESS_H
#define PACKER_ENDIENESS_H

enum class Endianness {
    BIG,
    LITTLE,
    NONE
};

Endianness endiannessDetect();

#endif //PACKER_ENDIENESS_H

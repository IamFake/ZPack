#ifndef PACKER_ZPACK_ZSTD_H
#define PACKER_ZPACK_ZSTD_H

#include <string>
#include <zstd.h>
#include <zbuff.h>
#include <zdict.h>
#include <iostream>
#include <tuple>
#include <cstdlib>
#include <cstring>
#include <functional>
#include "zpack_compression.h"

class zpack_zstd : public zpack_compression {
public:
    unsigned long long
    getCompressedSize(size_t size) override;

    unsigned long long
    compressBlock(const char *ibuf, size_t isize, char *obuf, size_t osize) override;

    unsigned long long
    getDecompressedSize(const char *ibuf, size_t isize) override;

    unsigned long long
    decompressBlock(const char *ibuf, size_t isize, char *obuf, size_t osize) override;

    bool streamCompressSetup() override;

    void streamCompressConsume(std::ostream &write, const char *buf, size_t size) override;

    void streamCompressEnd(std::ostream &write) override;

    bool streamDecompressSetup() override;

    void streamDecompressConsume(std::ostream &write, const char *buf, size_t size) override;

    void streamDecompressConsume(std::ostream &write, const char *buf, size_t size,
                                 std::function<void(const char *, size_t)> fn) override;

    bool streamDecompressEnd() override;
};


#endif //PACKER_ZPACK_ZSTD_H

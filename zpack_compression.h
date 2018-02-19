//
// Created by neos on 12/24/17.
//

#ifndef PACKER_ZPACK_COMPRESSION_H
#define PACKER_ZPACK_COMPRESSION_H


#include <string>
#include <zstd.h>
#include <iostream>
#include <tuple>
#include <cstdlib>
#include <boost/crc.hpp>
#include <functional>

class zpack_compression {
protected:
    short int compressionLevel = 19;

    char streamType = 'n';
    size_t streamBufSize = 0;
    void *streamBuf = nullptr;
    ZSTD_CStream *zstd_cStream = nullptr;
    ZSTD_DStream *zstd_dStream = nullptr;
    unsigned long long streamCompressed = 0;
    unsigned long long streamDecompressed = 0;

public:
    size_t streamDecompressLastConsume = 0;

    zpack_compression() = default;

    virtual ~zpack_compression() = default;

    unsigned long long getStreamCompressBytes();

    unsigned long long getStreamDecompressBytes();

    unsigned long long getStreamDecompressLastBytes();

    virtual unsigned long long getCompressedSize(size_t size) = 0;

    virtual unsigned long long compressBlock(const char *ibuf, size_t isize, char *obuf, size_t osize) = 0;

    virtual unsigned long long getDecompressedSize(const char *ibuf, size_t isize) = 0;

    virtual unsigned long long decompressBlock(const char *ibuf, size_t isize, char *obuf, size_t osize) = 0;

    virtual bool streamCompressSetup() = 0;

    virtual void streamCompressConsume(std::ostream &write, const char *buf, size_t size) = 0;

    virtual void streamCompressEnd(std::ostream &write) = 0;

    virtual bool streamDecompressSetup() = 0;

    virtual void streamDecompressConsume(std::ostream &write, const char *buf, size_t size) = 0;

    virtual void streamDecompressConsume(std::ostream &write, const char *buf, size_t size, std::function<void(const char *, size_t)> fn) = 0;

    virtual bool streamDecompressEnd() = 0;
};

#endif //PACKER_ZPACK_COMPRESSION_H

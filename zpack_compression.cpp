//
// Created by neos on 12/24/17.
//

#include "zpack_compression.h"

unsigned long long zpack_compression::getStreamCompressBytes() {
    return streamCompressed;
}

unsigned long long zpack_compression::getStreamDecompressBytes() {
    return streamDecompressed;
}

unsigned long long zpack_compression::getStreamDecompressLastBytes() {
    return streamDecompressLastConsume;
}

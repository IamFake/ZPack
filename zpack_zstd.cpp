#include "zpack_zstd.h"
#include "_cfg.h"

unsigned long long zpack_zstd::getCompressedSize(size_t size) {
    return ZSTD_compressBound(size);
}

unsigned long long zpack_zstd::compressBlock(const char *ibuf, size_t isize, char *obuf, size_t osize) {
    size_t compressed_len = ZSTD_compress(
        obuf, osize,
        ibuf, isize,
        compressionLevel
    );

    #if ZPACK_DEBUG
    std::cout << std::endl << "ZSTD in: " << isize << " out: " << compressed_len << std::endl << std::endl;
    #endif

    std::string errorDesc;
    if (ZSTD_isError(compressed_len)) {
        errorDesc = ZSTD_getErrorName(compressed_len);
        throw std::runtime_error(std::string("zpack_zstd::compressBlock error: ") + errorDesc);
    }

    return compressed_len;
}

unsigned long long zpack_zstd::getDecompressedSize(const char *ibuf, size_t isize) {
    unsigned long long rsize = ZSTD_getFrameContentSize(ibuf, isize);
    if (ZSTD_isError(rsize)) {
        throw std::runtime_error(
            std::string("zpack_zstd::getDecompressedSize:@ZSTD_getFrameContentSize: ") + ZSTD_getErrorName(rsize));
    }

    return rsize;
}

unsigned long long zpack_zstd::decompressBlock(const char *ibuf, size_t isize, char *obuf, size_t osize) {
    size_t decompressed_len = ZSTD_decompress(
        obuf, osize,
        ibuf, isize
    );

    std::string errorDesc;
    switch (decompressed_len) {
        case ZSTD_CONTENTSIZE_UNKNOWN:
            errorDesc = "The size cannot be determined";
            break;
        case ZSTD_CONTENTSIZE_ERROR:
            errorDesc = "an error occurred (invalid magic number, srcSize too small, or other)";
            break;
    }

    if (ZSTD_isError(decompressed_len)) {
        throw std::runtime_error(
            std::string("zpack_zstd::decompressBlock: ") + ZSTD_getErrorName(decompressed_len));
    }

    if (!errorDesc.empty()) {
        throw std::runtime_error(std::string("zpack_zstd::decompressBlock: ") + errorDesc);
    }

    return decompressed_len;
}

bool zpack_zstd::streamCompressSetup() {
    if (streamType == 'D')
        return false;

    streamType = 'C';

    streamBufSize = ZSTD_CStreamOutSize();
    streamBuf = std::malloc(streamBufSize);
    if (!streamBuf) {
        std::perror("zpack_zstd streamSetup buffer Out malloc failed");
        return false;
    }

    zstd_cStream = ZSTD_createCStream();
    if (zstd_cStream == NULL) {
        throw std::runtime_error("ZSTD_createCStream() error");
    }

    size_t init_result = ZSTD_initCStream(zstd_cStream, compressionLevel);
    if (ZSTD_isError(init_result)) {
        throw std::runtime_error(std::string("ZSTD_initCStream error: ") + ZSTD_getErrorName(init_result));
    }

    return true;
}

void zpack_zstd::streamCompressConsume(std::ostream &write, const char *buf, size_t size) {
    ZSTD_inBuffer input{buf, size, 0};

    while (input.pos < input.size) {
        ZSTD_outBuffer output{streamBuf, streamBufSize, 0};

        auto readed = ZSTD_compressStream(zstd_cStream, &output, &input);
        if (ZSTD_isError(readed)) {
            throw std::runtime_error(
                std::string("zpack_zstd::streamCompressConsume error: ") + ZSTD_getErrorName(readed));
        }

        write.write((char *) streamBuf, output.pos);
        streamCompressed += output.pos;
    }
}

void zpack_zstd::streamCompressEnd(std::ostream &write) {
    ZSTD_outBuffer output{streamBuf, streamBufSize, 0};
    size_t const left = ZSTD_endStream(zstd_cStream, &output);
    if (left > 0) {
        if (ZSTD_isError(left)) {
            throw std::runtime_error(
                std::string("zpack_zstd::streamCompressEnd error: ") + ZSTD_getErrorName(left));
        } else {
            throw std::runtime_error("zpack_zstd::streamCompressEnd error: stream flush is not complete");
        }
    }

    write.write((char *) streamBuf, output.pos);
    streamCompressed += output.pos;

    std::free(streamBuf);
    ZSTD_freeCStream(zstd_cStream);
}

bool zpack_zstd::streamDecompressSetup() {
    if (streamType == 'C')
        return false;

    streamType = 'D';

    streamBufSize = ZSTD_DStreamOutSize();
    streamBuf = std::malloc(streamBufSize);
    if (!streamBuf) {
        std::perror("zpack_zstd streamSetup buffer Out malloc failed");
        return false;
    }

    zstd_dStream = ZSTD_createDStream();
    if (zstd_dStream == NULL) {
        std::cerr << "ZSTD_createDStream() error" << std::endl;
        return false;
    }

    size_t init_result = ZSTD_initDStream(zstd_dStream);
    if (ZSTD_isError(init_result)) {
        std::cerr << "ZSTD_initDStream error: " << ZSTD_getErrorName(init_result) << std::endl;
        return false;
    }

    return true;
}

void zpack_zstd::streamDecompressConsume(std::ostream &write, const char *buf, size_t size) {
    streamDecompressConsume(write, buf, size, nullptr);
}

void zpack_zstd::streamDecompressConsume(std::ostream &write, const char *buf, size_t size,
                                         std::function<void(const char *, size_t)> fn) {
    ZSTD_inBuffer input{buf, size, 0};
    size_t session_size = 0;

    while (input.pos < input.size) {
        ZSTD_outBuffer output{streamBuf, streamBufSize, 0};

        auto readed = ZSTD_decompressStream(zstd_dStream, &output, &input);
        if (ZSTD_isError(readed)) {
            throw std::runtime_error(
                std::string("zpack_zstd::streamDecompressConsume error: ") + ZSTD_getErrorName(readed));
        }

        write.write((char *) streamBuf, output.pos);
        if (fn != nullptr) {
            fn((const char *) streamBuf, output.pos);
        }

        session_size += output.pos;

        streamDecompressed += output.pos;
        streamDecompressLastConsume = session_size;
        #if ZPACK_DEBUG
        std::cout << "===== streamDecompressConsume "
                  << " out pos: " << output.pos << " out size: " << output.size
                  << " inp pos: " << input.pos << " inp size: " << input.size
                  << " ret: "
                  << readed// << " CTX: " << ZSTD_sizeof_DStream(zstd_dStream)
                  << " sess: " << session_size
                  << std::endl;
        #endif
    }
}

bool zpack_zstd::streamDecompressEnd() {
    std::free(streamBuf);
    ZSTD_freeDStream(zstd_dStream);

    return true;
}
#ifndef ZPACK_LIBRARY_H
#define ZPACK_LIBRARY_H

typedef unsigned int uint;
typedef unsigned short usint;
typedef unsigned long ulint;
typedef unsigned long long ullint;
typedef long long llint;
typedef unsigned char uchar;

#include <fstream>
#include <iostream>
#include <unordered_map>
#include "boost/filesystem.hpp"
#include "boost/crc.hpp"
#include "_prepare_int.h"
#include "zpack_compression.h"
#include "zpack_zstd.h"

namespace fs = boost::filesystem;

struct LocalFileHeaderRecord {
    uchar signature[4];
    uchar version[2];
    uchar general[2];
    uchar compression[2];
    uchar filenameLen[2];
    uchar crc32[4];
    uchar compressedSize[8];
    uchar uncompressedSize[8];
    uchar mtime[8];
    uchar offsetGap[8];
    uchar extraLen[2];

    void write(std::fstream &stream) const {
        stream.write((const char *) this, sizeof(*this));
    }

    void read(std::fstream &stream) {
        stream.read((char *) this, sizeof(*this));
    }

    uint getSignature() const {
        return readInt<uint>(signature);
    }

    usint getVersion() const {
        return readInt<usint>(version);
    }

    usint getGeneral() const {
        return readInt<usint>(general);
    }

    usint getCompression() const {
        return readInt<usint>(compression);
    }

    usint getFilenameLen() const {
        return readInt<usint>(filenameLen);
    }

    uint getCrc32() const {
        return readInt<uint>(crc32);
    }

    ullint getCompressedSize() const {
        return readInt<ullint>(compressedSize);
    }

    ullint getUncompressedSize() const {
        return readInt<ullint>(uncompressedSize);
    }

    llint getMtime() const {
        return readInt<llint>(mtime);
    }

    ullint getOffsetGap() const {
        return readInt<ullint>(offsetGap);
    }

    usint getExtraLen() const {
        return readInt<usint>(extraLen);
    }
};

struct LocalFileExtraField {
    uchar id[2];
    uchar value[2];

    void write(std::fstream &stream) const {
        stream.write((const char *) this, sizeof(*this));
    }

    void read(std::fstream &stream) {
        stream.read((char *) this, sizeof(*this));
    }

    usint getId() const {
        return readInt<usint>(id);
    }

    usint getValue() const {
        return readInt<usint>(value);
    }
};

struct DirectoryFileHeaderRecord {
    uchar signature[4];
    uchar versionBy[2];
    uchar versionMin[2];
    uchar general[2];
    uchar compressMethod[2];
    uchar crc32[4];
    uchar mtime[8];
    uchar compressedSize[8];
    uchar uncompressedSize[8];
    uchar offsetFile[8];
    uchar offsetRecord[8];
    uchar filenameLen[2];
    uchar extraLen[2];
    uchar commentLen[2];
    uchar attrsInternal[2];
    uchar attrsExternal[4];

    void write(std::fstream &stream) const {
        stream.write((const char *) this, sizeof(*this));
    }

    void read(std::fstream &stream) {
        stream.read((char *) this, sizeof(*this));
    }

    uint getSignature() const {
        return readInt<uint>(signature);
    }

    usint getVersionBy() const {
        return readInt<usint>(versionBy);
    }

    usint getVersionMin() const {
        return readInt<usint>(versionMin);
    }

    usint getGeneral() const {
        return readInt<usint>(general);
    }

    usint getCompressMethod() const {
        return readInt<usint>(compressMethod);
    }

    uint getCrc32() const {
        return readInt<uint>(crc32);
    }

    llint getMtime() const {
        return readInt<llint>(mtime);
    }

    ullint getCompressedSize() const {
        return readInt<ullint>(compressedSize);
    }

    ullint getUncompressedSize() const {
        return readInt<ullint>(uncompressedSize);
    }

    ullint getOffsetFile() const {
        return readInt<ullint>(offsetFile);
    }

    ullint getOffsetRecord() const {
        return readInt<ullint>(offsetRecord);
    }

    usint getFilenameLen() const {
        return readInt<usint>(filenameLen);
    }

    usint getExtraLen() const {
        return readInt<usint>(extraLen);
    }

    usint getCommentLen() const {
        return readInt<usint>(commentLen);
    }

    usint getAttrsInternal() const {
        return readInt<usint>(attrsInternal);
    }

    uint getAttrsExternal() const {
        return readInt<uint>(attrsExternal);
    }
};

struct DirectoryFileQueue {
    DirectoryFileHeaderRecord record;
    std::vector<LocalFileExtraField> extra;
    std::string filename;
    std::string comment;
};

struct EndOfDirectoryRecord {
    uchar signature[4];
    uchar recordsNumber[2];
    uchar commentLen[2];
    uchar dirRecordOffset[8];
    uchar dirRecordSize[4];

    uint getSignature() {
        return readInt<uint>(signature);
    }

    usint getRecordsNumber() {
        return readInt<usint>(recordsNumber);
    }

    ullint getRecordOffset() {
        return readInt<ullint>(dirRecordOffset);
    }

    uint getRecordSize() {
        return readInt<uint>(dirRecordSize);
    }

    usint getCommentLen() {
        return readInt<usint>(commentLen);
    }

    void write(std::fstream &stream) const {
        stream.write((const char *) this, sizeof(*this));
    }

    void read(std::fstream &stream) {
        stream.read((char *) this, sizeof(*this));
    }
};

struct ZPackStats {
    ullint filesSizeUncompressed;
    ullint filesSizeCompressed;
    ullint archiveSize;
    uint records;
    ullint lastOffset;
    ullint directoryOffset;
};

class ZPack {
    ullint borderOffset = 0;
    std::fstream file;
    std::string archive_name;
    std::unordered_map<std::string, DirectoryFileQueue> list;
    ZPackStats stats{0, 0, 0, 0, 0, 0};

    fs::path rootPath;

    uint blockSizeMax = 1024 * 1024 * 6;
    uint blockSizeBytes = blockSizeMax;

    bool shouldRepack = false;

    enum Signatures {
        LocalHeader = 0x0201534e,
        DirectoryEntry = 0x0605534e,
        DirectoryRecord = 0x0807534e
    };
    enum ExtraFlags {
        Permissions = 1
    };
    enum GeneralFlags {
        Streamed = 1
    };
    enum Compression {
        CompressNone = 0,
        CompressZstd,
        CompressZstdStream
    };

    EndOfDirectoryRecord dir_end{};

public:
    static const short version = 1;
    static const short versionMin = 1;

    enum class Errors {
        OK,
        ERR_READ_DIRECTORY_END,
        ERR_DIRECTORY_END_SIGNATURE,
        ERR_OPENING_ARCHIVE_FILE,
        ERR_OPENING_REPACK_FILE,
        ERR_READ_ENTRY_HEADER,
        ERR_READ_ENTRY_NAME,
        ERR_READ_ENTRY_EXTRA,
        ERR_READ_ENTRY_COMMENT,
        ERR_READ_LOCAL_HEADER,
        ERR_PACK_FILE_OPEN,
        ERR_PACK_ITEM_SIZE,
        ERR_EXTRACT_GENERAL,
        ERR_WRITE_WRONG_SEEK,
        ERR_UNKNOWN
    };
    Errors error_code = Errors::OK;

    ZPack() = default;

    ~ZPack();

    ZPack *open(const char *filename, bool trunicate = false);

    void close();

    void clear();

    void write();

    bool packFile(std::string const &filename, std::string const &directory = "", std::string const &comment = "");

    bool packItem(std::string const &itemname, std::string const &data, std::string const &directory = "",
                  const std::string &comment = "");

    bool remove(std::string const &name);

    bool extractFile(std::string const &name, std::string const &dest);

    std::string extractStr(std::string const &name);

    void repack();

    ZPackStats getStats();

    bool good();

    bool fail();

    bool bad();

private:

    bool packData(std::istream &stream, std::string const &itemname, fs::perms &perms, ullint fileSize = 0,
                  llint modificationTime = 0, std::string const &comment = "",
                  Compression compress_method = CompressZstd);

    bool extract(DirectoryFileQueue &sitem, std::ostream &stream);

    usint readDirectory();

    ullint writeDirectory(std::fstream &stream);

    std::unique_ptr<zpack_compression> createCompression(Compression &method);
};

#endif
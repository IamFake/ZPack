#include <chrono>
#include <sstream>
#include "zpack.h"
#include "_cfg.h"

ZPack::~ZPack() {
    close();
    list.clear();
}

ZPackStats ZPack::getStats() {
    return stats;
}

void ZPack::write() {
    if (!file.is_open())
        return;

    ullint offset_diff = writeDirectory(file);
    file.flush();
    if (offset_diff > 0) {
        file.close();
        file.clear();

        fs::resize_file(archive_name, offset_diff);
        open(archive_name.c_str());
    }
}

void ZPack::close() {
    if (file.is_open()) {
        file.close();
        file.clear();
    }
}

void ZPack::clear() {
    error_code = Errors::OK;
}

std::unique_ptr<zpack_compression> ZPack::createCompression(Compression &method) {
    std::unique_ptr<zpack_compression> ar_ptr = nullptr;
    if (method == CompressZstd || method == CompressZstdStream) {
        ar_ptr = std::unique_ptr<zpack_compression>(new zpack_zstd());
    }

    return ar_ptr;
}

ZPack *ZPack::open(const char *filename_to_open, bool trunicate) {
    archive_name = filename_to_open;
    auto flags = std::ios_base::binary | std::ios_base::in | std::ios_base::out | std::ios_base::ate;
    if (trunicate) {
        flags |= std::ios_base::trunc;
    }

    rootPath = fs::path(archive_name).remove_filename();

    file.open(archive_name, flags);
    if (!file.is_open()) {
        file.clear();
        file.open(archive_name, std::ios_base::out);
        file.close();
        file.open(archive_name, flags);
    }

    if (file.fail()) {
        error_code = Errors::ERR_OPENING_ARCHIVE_FILE;
        std::cerr << "ZPack::open failed: " << errno << " msg: " << strerror(errno) << std::endl;
    }

    #if ZPACK_DEBUG
    std::cout << std::endl << "Opening archive with" << std::endl
              << "dirOffset: " << dir_end.getRecordOffset() << std::endl
              << "tellp:     " << file.tellp() << std::endl
              << "tellg:     " << file.tellg() << std::endl
              << "failbit:   " << file.fail() << std::endl
              << "badbit:    " << file.bad() << std::endl
              << "eofbit:    " << file.eof() << std::endl
              << "goodbit:   " << file.good() << std::endl
              << std::endl;
    #endif

    if (file.is_open() && !trunicate) {
        auto firstTellg = file.tellg();
        if (firstTellg != -1) {
            // last offset point based on std::ios_base::ate flag
            borderOffset = (ullint) firstTellg;
        }

        if (firstTellg > 0) {
            auto rd = readDirectory();
            if (rd > 0) {
                std::cerr << "Reading directory failed: " << std::endl
                          << "error:    " << rd << std::endl << std::endl;
            }
        }
    }

    return this;
}

usint ZPack::readDirectory() {
    file.seekg(-sizeof(EndOfDirectoryRecord), std::ios_base::end);
    dir_end.read(file);
    if ((int) sizeof(EndOfDirectoryRecord) > file.gcount()) {
        error_code = Errors::ERR_READ_DIRECTORY_END;
        return 1;
    }

    #if ZPACK_DEBUG
    std::cout
        << "READ DIR: " << file.gcount() << " : " << sizeof(EndOfDirectoryRecord) << std::endl << std::endl
        << "Directory End Record: " << std::endl
        << "SIZE :   " << sizeof(EndOfDirectoryRecord) << " ALIGN " << alignof(EndOfDirectoryRecord) << std::endl
        << "IS POD:  " << std::is_pod<EndOfDirectoryRecord>::value << std::endl
        << "IS TRIV: " << std::is_trivial<EndOfDirectoryRecord>::value << std::endl
        << "signature: " << std::hex << dir_end.getSignature() << std::endl << std::dec
        << "recordsNumber: " << dir_end.getRecordsNumber() << std::endl
        << "dirRecordSize: " << dir_end.getRecordSize() << std::endl
        << "dirRecordOffset: " << dir_end.getRecordOffset() << std::endl
        << "commentLen: " << dir_end.getCommentLen() << std::endl << std::endl;
    #endif

    if (dir_end.getSignature() != DirectoryRecord) {
        dir_end = {};
        error_code = Errors::ERR_DIRECTORY_END_SIGNATURE;
        return 1;
    }

    file.clear();

    if (dir_end.getRecordsNumber() == 0)
        return 0;

    list.clear();
    file.seekg(dir_end.getRecordOffset());

    if (!file) {
        error_code = Errors::ERR_OPENING_ARCHIVE_FILE;
        return 1;
    }

    for (uint i = 0; i < dir_end.getRecordsNumber(); i++) {
        DirectoryFileHeaderRecord dfq{};
        dfq.read(file);
        if (file.gcount() != sizeof(dfq)) {
            error_code = Errors::ERR_READ_ENTRY_HEADER;
            return 1;
        }

        auto filenameLen = dfq.getFilenameLen();
        std::string filename;
        if (filenameLen > 0) {
            char *sbuf = new char[filenameLen + 1]();
            file.read(sbuf, (int) filenameLen);
            filename.reserve(filenameLen);
            filename = sbuf;
            delete[] sbuf;
            if (file.gcount() != filenameLen) {
                error_code = Errors::ERR_READ_ENTRY_NAME;
                return 1;
            }
        }

        auto extraLen = dfq.getExtraLen();
        usint extraEntries = extraLen / sizeof(LocalFileExtraField);
        std::vector<LocalFileExtraField> extraItems;
        for (uint ex = 0; ex < extraEntries; ex++) {
            LocalFileExtraField lfef{};
            lfef.read(file);
            if (file.gcount() != sizeof(lfef)) {
                error_code = Errors::ERR_READ_ENTRY_EXTRA;
                return 1;
            }

            extraItems.push_back(lfef);
        }

        std::string comment;
        auto commentLen = dfq.getCommentLen();
        if (commentLen > 0) {
            char *sbuf = new char[commentLen + 1]();
            file.read(sbuf, commentLen);
            comment.reserve(commentLen);
            comment = sbuf;
            delete[] sbuf;
            if (file.gcount() != commentLen) {
                error_code = Errors::ERR_READ_ENTRY_COMMENT;
                return 1;
            }
        }

        #if ZPACK_DEBUG
        std::cout << "Read dir entry header: " << std::hex << std::endl
                  << "signature:        " << dfq.getSignature() << std::endl << std::dec
                  << "filename:         " << filename << std::endl
                  << "filenameReal:     " << filename.size() << std::endl
                  << "filenameLen:      " << dfq.getFilenameLen() << std::endl
                  << "lastRead:         " << file.gcount() << std::endl
                  << "versionBy:        " << dfq.getVersionBy() << std::endl
                  << "versionMin:       " << dfq.getVersionMin() << std::endl
                  << "general:          " << dfq.getGeneral() << std::endl
                  << "compressMethod:   " << dfq.getCompressMethod() << std::endl
                  << "mtime:            " << dfq.getMtime() << std::endl
                  << "compressedSize:   " << dfq.getCompressedSize() << std::endl
                  << "uncompressedSize: " << dfq.getUncompressedSize() << std::endl
                  << "offsetFile:       " << dfq.getOffsetFile() << std::endl
                  << "offsetRecord:     " << dfq.getOffsetRecord() << std::endl
                  << "crc32:            " << dfq.getCrc32() << std::endl
                  << "commentLen:       " << dfq.getCommentLen() << std::endl
                  << "comment:          " << comment << std::endl << std::endl;
        #endif

        list.insert({
                        filename,
                        DirectoryFileQueue{
                            dfq,
                            extraItems,
                            filename,
                            comment
                        }
                    });
    }

    file.seekg(0);
    file.seekp(dir_end.getRecordOffset());
    return 0;
}

bool ZPack::packFile(std::string const &filename, std::string const &directory, const std::string &comment) {
    auto fsize = (ullint) fs::file_size(filename);
    auto mtime = (llint) fs::last_write_time(filename);
    auto perms = fs::status(filename).permissions();

    auto path = fs::path(filename);
    std::string itemname =
        directory + (directory.back() != '/' && !directory.empty() ? "/" : "") + path.filename().string();

    #if ZPACK_DEBUG
    std::cout << "Dirs variants: " << std::endl
              << "buf size max  : " << blockSizeMax << std::endl
              << "orig file     : " << filename << std::endl
              << "root path     : " << rootPath.string() << std::endl
              << "bst path      : " << path.string() << std::endl
              << "bst path2     : " << itemname << std::endl << std::endl;
    #endif

    std::ifstream sfile(filename, std::ios_base::binary | std::ios_base::in);
    if (!sfile.is_open()) {
        error_code = Errors::ERR_PACK_FILE_OPEN;
        return false;
    }

    return this->packData(
        sfile,
        itemname,
        perms,
        fsize,
        mtime,
        comment
    );
}

bool ZPack::packItem(std::string const &itemname, std::string const &data, std::string const &directory,
                     const std::string &comment) {
    llint mtime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    fs::perms perms =
        fs::perms::owner_read |
        fs::perms::owner_write |
        fs::perms::others_read;
    ullint dataSize = data.size();
    std::istringstream sfile(data);

    std::string itemname_normalized = directory + (directory.back() != '/' && !directory.empty() ? "/" : "") + itemname;

    if (dataSize == 0) {
        error_code = Errors::ERR_PACK_ITEM_SIZE;
        return false;
    }

    return this->packData(
        sfile,
        itemname_normalized,
        perms,
        dataSize,
        mtime,
        comment
    );
}

bool ZPack::packData(
    std::istream &stream,
    std::string const &itemname,
    fs::perms &perms,
    ullint fileSize,
    llint modificationTime,
    const std::string &comment,
    Compression compress_method
) {
    if (stream.good() && file.good()) {
        {
            auto existed = list.find(itemname);
            if (existed != list.end()) {
                if (
                    existed->second.record.getUncompressedSize() == fileSize &&
                    existed->second.record.getMtime() == modificationTime
                    ) {
                    return true;
                }
            }
        }

        ullint offset_start = dir_end.getRecordOffset();
        ullint offset_end = 0;
        usint general_flag = 0;
        bool single_step = false;

        uint ibufSize = blockSizeBytes;
        if (ibufSize > blockSizeMax) ibufSize = blockSizeMax;
        char *ibuf = new char[ibufSize];
        char *obuf = nullptr;
        ullint compressedSize = fileSize;

        std::unique_ptr<zpack_compression> ar = nullptr;

        boost::crc_32_type crc32;
        uint crc32_result = 0;

        LocalFileExtraField extra_perms{};
        assignInt<usint>(Permissions, extra_perms.id);
        assignInt<usint>(perms, extra_perms.value);

        LocalFileHeaderRecord loc_hd{};
        assignInt<uint>(LocalHeader, loc_hd.signature);
        assignInt<usint>(version, loc_hd.version);
        assignInt<usint>(general_flag, loc_hd.general);
        assignInt<usint>(compress_method, loc_hd.compression);
        assignInt<llint>(modificationTime, loc_hd.mtime);
        assignInt<uint>(0, loc_hd.crc32);
        assignInt<ullint>(fileSize, loc_hd.compressedSize);
        assignInt<ullint>(fileSize, loc_hd.uncompressedSize);
        assignInt<usint>((usint) itemname.size(), loc_hd.filenameLen);
        assignInt<usint>(sizeof(extra_perms), loc_hd.extraLen);
        assignInt<ullint>(0, loc_hd.offsetGap);

        if (fileSize <= ibufSize) {
            single_step = true;
            if (fileSize <= 80) {
                compress_method = CompressNone;
            }

            stream.read(ibuf, ibufSize);
            if (compress_method != CompressNone) {
                ar = createCompression(compress_method);
                compressedSize = ar->getCompressedSize(fileSize);
                obuf = new char[compressedSize];

                compressedSize = ar->compressBlock(ibuf, (size_t) stream.gcount(), obuf, compressedSize);
            }

            crc32.process_bytes(ibuf, (size_t) stream.gcount());
            crc32_result = crc32.checksum();

            assignInt<uint>(crc32_result, loc_hd.crc32);
            assignInt<ullint>(compressedSize, loc_hd.compressedSize);
            assignInt<usint>(compress_method, loc_hd.compression);
        } else {
            general_flag |= Streamed;

            ar = createCompression(compress_method);

            assignInt<ullint>(0, loc_hd.uncompressedSize);
            assignInt<ullint>(0, loc_hd.compressedSize);
            assignInt<usint>(general_flag, loc_hd.general);
        }

        #if ZPACK_DEBUG
        std::cout << "PACK DATA " << itemname << " gen size " << sizeof(LocalFileHeaderRecord) << " size "
                  << sizeof(loc_hd) << " align " << alignof(loc_hd) << std::endl;
        #endif

        file.seekp(offset_start);
        loc_hd.write(file);
        file.write(itemname.c_str(), itemname.size());
        file.write((const char *) &extra_perms, sizeof(extra_perms));

        auto fileOffset = file.tellp();

        if (single_step) {
            if (compress_method != CompressNone) {
                file.write(obuf, (std::streamsize) compressedSize);
            } else {
                file.write(ibuf, stream.gcount());
            }
        } else {
            if (compress_method != CompressNone) ar->streamCompressSetup();

            while (stream.good() && file.good()) {
                stream.read(ibuf, ibufSize);
                if (compress_method != CompressNone) {
                    ar->streamCompressConsume(file, ibuf, (size_t) stream.gcount());
                } else {
                    file.write(ibuf, stream.gcount());
                }
                crc32.process_bytes(ibuf, (size_t) stream.gcount());
            }

            if (compress_method != CompressNone) ar->streamCompressEnd(file);

            crc32_result = crc32.checksum();

            assignInt<uint>(crc32_result, loc_hd.crc32);
            assignInt<ullint>(fileSize, loc_hd.uncompressedSize);
            if (compress_method != CompressNone) {
                assignInt<ullint>(ar->getStreamCompressBytes(), loc_hd.compressedSize);
            } else {
                assignInt<ullint>(fileSize, loc_hd.compressedSize);
            }

            ullint rewind = (ullint) file.tellp();
            file.seekp(offset_start);
            loc_hd.write(file);
            file.seekp(rewind);
        }

        offset_end = (ullint) file.tellp();

        DirectoryFileHeaderRecord dfhr{};
        assignInt<uint>(DirectoryEntry, dfhr.signature);
        assignInt<usint>(version, dfhr.versionBy);
        assignInt<usint>(versionMin, dfhr.versionMin);
        assignInt<usint>(general_flag, dfhr.general);
        assignInt<usint>(compress_method, dfhr.compressMethod);
        assignInt<llint>(modificationTime, dfhr.mtime);
        assignInt<uint>(crc32_result, dfhr.crc32);
        assignInt<ullint>(readInt<ullint>(loc_hd.compressedSize), dfhr.compressedSize);
        assignInt<ullint>(fileSize, dfhr.uncompressedSize);
        assignInt<usint>((usint) itemname.size(), dfhr.filenameLen);
        assignInt<usint>(sizeof(extra_perms), dfhr.extraLen);
        assignInt<usint>((usint) comment.size(), dfhr.commentLen);
        assignInt<usint>(0, dfhr.attrsInternal);
        assignInt<uint>(0, dfhr.attrsExternal);
        assignInt<ullint>((ullint) fileOffset, dfhr.offsetFile);
        assignInt<ullint>(offset_start, dfhr.offsetRecord);

        list[itemname] = DirectoryFileQueue{
            dfhr,
            {extra_perms},
            itemname,
            comment
        };

        delete[] ibuf;
        delete[] obuf;

        assignInt<ullint>(offset_end, dir_end.dirRecordOffset);

        return true;
    } else {
        if (!file.good()) {
            error_code = Errors::ERR_OPENING_ARCHIVE_FILE;
        } else {
            error_code = Errors::ERR_PACK_FILE_OPEN;
        }
    }

    return false;
}

bool ZPack::remove(std::string const &name) {
    auto res = list.erase(name) == 1;
    #if ZPACK_DEBUG
    std::cout << "Remove file from archive " << name << " result: " << res << std::endl;
    #endif
    return res;
}

bool ZPack::extract(DirectoryFileQueue &sitem, std::ostream &stream) {
    uint crc32_result = 0;
    uint ibufSize = blockSizeBytes;
    if (ibufSize > blockSizeMax) ibufSize = blockSizeMax;
    if (ibufSize > sitem.record.getCompressedSize()) ibufSize = (uint) sitem.record.getCompressedSize();
    char *ibuf = new char[ibufSize];
    char *obuf = nullptr;

    try {
        boost::crc_32_type crc32;
        ullint readed = 0;
        auto crc32_callback = [&crc32](const char *buf, size_t size) {
            crc32.process_bytes(buf, size);
        };

        file.seekg(sitem.record.getOffsetFile());
        Compression compress_method = (Compression) sitem.record.getCompressMethod();
        GeneralFlags general_flags = (GeneralFlags) sitem.record.getGeneral();

        auto ar = createCompression(compress_method);
        auto compressedFileSize = sitem.record.getCompressedSize();

        if (general_flags & Streamed) {
            ar->streamDecompressSetup();
        }

        while (readed < compressedFileSize) {
            ullint readed_left = compressedFileSize - readed;
            file.read(ibuf, (uint) (readed_left > ibufSize ? ibufSize : readed_left));

            readed += file.gcount();

            ullint d_size = 0;
            if (compress_method != CompressNone && !(general_flags & Streamed)) {
                auto d_predictSize = ar->getDecompressedSize(ibuf, (size_t) file.gcount());
                obuf = new char[d_predictSize];

                d_size = ar->decompressBlock(ibuf, (size_t) file.gcount(), obuf, d_predictSize);

                stream.write(obuf, (std::streamsize) d_size);
                crc32.process_bytes(obuf, (size_t) d_size);

                delete[] obuf;
            } else if (compress_method != CompressNone && general_flags & Streamed) {
                ar->streamDecompressConsume(stream, ibuf, (size_t) file.gcount(), crc32_callback);
                d_size = ar->getStreamDecompressLastBytes();
            } else {
                stream.write(ibuf, file.gcount());
                crc32.process_bytes(ibuf, (size_t) file.gcount());
            }

            #if ZPACK_DEBUG
            std::cout << "EXTRACTING: " << sitem.filename << " " << d_size << " from readed " << readed << std::endl;
            #endif
        }

        if (general_flags & Streamed) {
            ar->streamDecompressEnd();
        }

        crc32_result = crc32.checksum();
    } catch (std::runtime_error &e) {
        std::cerr << "zpack::extract: Error with filesystem operation: " << e.what() << std::endl;
        error_code = Errors::ERR_EXTRACT_GENERAL;
    } catch (...) {
        std::cerr << "zpack::extract: Error general" << std::endl;
        error_code = Errors::ERR_EXTRACT_GENERAL;
    };

    delete[] ibuf;

    #if ZPACK_DEBUG
    std::cout << "CHECK CRC32 " << crc32_result << " against " << sitem.record.getCrc32() << std::endl;
    #endif
    if (crc32_result != sitem.record.getCrc32()) {
        #if ZPACK_DEBUG
        std::cout << "WRONG CRC32 " << crc32_result << " against " << sitem.record.getCrc32() << std::endl;
        #endif
    }

    return true;
}

std::string ZPack::extractStr(std::string const &name) {
    auto item = list.find(name);
    if (item == list.end()) return "";

    DirectoryFileQueue &sitem = item->second;

    std::ostringstream stream;

    extract(sitem, stream);

    return stream.str();
}

bool ZPack::extractFile(std::string const &name, std::string const &dest) try {
    auto item = list.find(name);
    if (item == list.end()) return false;

    DirectoryFileQueue &sitem = item->second;

    auto extractPath = fs::path(dest);
    if (extractPath.empty()) return false;
    extractPath.remove_filename();
    fs::create_directory(extractPath);

    extractPath /= sitem.filename;
    extractPath.lexically_normal();

    {
        fs::path doublePath = extractPath;
        doublePath.remove_filename();
        fs::create_directories(doublePath);
    }

    std::ofstream wfile(extractPath.c_str(), std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);

    extract(sitem, wfile);

    fs::perms perms = fs::perms::owner_read |
                      fs::perms::owner_write |
                      fs::perms::others_read;

    for (LocalFileExtraField &extra_item : sitem.extra) {
        if (extra_item.getId() == Permissions) {
            perms = (fs::perms) extra_item.getValue();
        }
    }

    fs::status(extractPath).permissions(perms);

    return true;
} catch (fs::filesystem_error &e) {
    std::cerr << "extractFile: Error with fs operation: " << e.what();
    error_code = Errors::ERR_EXTRACT_GENERAL;
    return false;
} catch (...) {
    std::cerr << "extractFile: General error when extracting...";
    error_code = Errors::ERR_EXTRACT_GENERAL;
    return false;
}

void ZPack::repack() {
    if (!file.is_open()) {
        error_code = Errors::ERR_OPENING_ARCHIVE_FILE;
        return;
    }

    std::string repack_file = archive_name + "r";
    std::fstream rfile(repack_file, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    if (!rfile) {
        error_code = Errors::ERR_OPENING_REPACK_FILE;
        return;
    }

    for (auto &item : list) {
        const std::string &name = item.first;
        DirectoryFileQueue &data = item.second;

        ullint moved = 0;
        ullint moved_max =
            data.record.getCompressedSize() + sizeof(LocalFileHeaderRecord) + data.record.getFilenameLen() +
            data.record.getExtraLen();

        #if ZPACK_DEBUG
        std::cout << "Repack file " << name << " with struct size " << sizeof(data.record) << " ("
                  << sizeof(DirectoryFileHeaderRecord)
                  << ")" << " align " << alignof(data.record) << std::endl
                  << "filesize: " << data.record.getCompressedSize()
                  << std::endl;
        #endif

        uint bufSize = blockSizeBytes;
        if (bufSize > blockSizeMax) bufSize = blockSizeMax;
        if (bufSize > moved_max) bufSize = (uint) moved_max;
        char *buf = new char[bufSize];

        try {
            file.seekg(data.record.getOffsetRecord());

            DirectoryFileHeaderRecord check_rec{};
            check_rec.read(file);
            if (check_rec.getSignature() != LocalHeader) {
                error_code = Errors::ERR_READ_LOCAL_HEADER;
                return;
            }

            file.seekg(data.record.getOffsetRecord());
            assignInt<ullint>((ullint) rfile.tellp(), data.record.offsetRecord);
            assignInt<ullint>((ullint) rfile.tellp() + sizeof(LocalFileHeaderRecord) + data.record.getFilenameLen() +
                              data.record.getExtraLen(), data.record.offsetFile);
            while (rfile && file && moved < moved_max) {
                ullint moved_left = moved_max - moved;
                file.read(buf, (uint) (moved_left > bufSize ? bufSize : moved_left));
                moved += file.gcount();
                rfile.write(buf, file.gcount());
            }
        } catch (...) {
            error_code = Errors::ERR_UNKNOWN;
            std::cerr << "REPACK ERROR..." << std::endl
                      << "file:  " << file.good() << std::endl
                      << "rfile: " << rfile.good() << std::endl
                      << std::endl;
        }

        #if ZPACK_DEBUG
        std::cout << "Repack file " << name << " moved " << moved << std::endl << std::endl;
        #endif

        delete[] buf;
    }

    this->writeDirectory(rfile);

    rfile.flush();
    rfile.close();
    file.close();
    file.clear();

    fs::rename(repack_file, archive_name);

    open(archive_name.c_str());
}

ullint ZPack::writeDirectory(std::fstream &stream) {
    if (&stream == &file) {
        // that's mean that is not a "repack" operation
        stream.seekp(dir_end.getRecordOffset());
    }

    #if ZPACK_DEBUG
    std::cout << "WRITE DIRECTIRY END. tellp: " << stream.tellp() << " tellg: " << stream.tellg() << std::endl;
    #endif

    if (stream.tellp() == -1) {
        error_code = Errors::ERR_WRITE_WRONG_SEEK;
        return 0;
    }

    stats = {0, 0, 0, 0, 0, 0};
    uint dirSize = 0;
    ullint localsSize = 0;
    auto dirOffset = (ullint) stream.tellp();
    for (auto &item : list) {
        const std::string &name = item.first;
        DirectoryFileQueue &data = item.second;

        stats.filesSizeCompressed += data.record.getCompressedSize();
        stats.filesSizeUncompressed += data.record.getUncompressedSize();

        data.record.write(stream);
        stream.write(name.c_str(), name.size());

        for (LocalFileExtraField const &exItem : data.extra) {
            exItem.write(stream);
            dirSize += sizeof(exItem);
            localsSize += sizeof(exItem);
        }

        stream.write(data.comment.c_str(), data.comment.size());

        #if ZPACK_DEBUG
        std::cout << "pass: " << data.filename << std::endl
                  << data.comment << " ^ " << data.comment.size() << std::endl
                  << data.record.commentLen
                  << std::endl << std::endl;
        #endif

        dirSize += sizeof(data.record);
        dirSize += data.filename.size();
        dirSize += data.comment.size();

        localsSize += data.filename.size();
        localsSize += data.comment.size();
        localsSize += sizeof(LocalFileHeaderRecord);
    }

    EndOfDirectoryRecord eodr{};
    assignInt<uint>(DirectoryRecord, eodr.signature);
    assignInt<usint>((usint) list.size(), eodr.recordsNumber);
    assignInt<uint>(dirSize, eodr.dirRecordSize);
    assignInt<ullint>(dirOffset, eodr.dirRecordOffset);
    assignInt<usint>(0, eodr.commentLen);

    eodr.write(stream);
    dir_end = eodr;

    stats.archiveSize = stats.filesSizeCompressed + dirSize + sizeof(eodr) + localsSize;
    stats.records = (uint) list.size();
    auto lastOffset = (ullint) stream.tellp();
    stats.lastOffset = lastOffset;
    stats.directoryOffset = eodr.getRecordOffset();

    #if ZPACK_DEBUG
    std::cout << "Close #2 directory record with: " << std::endl
              << "dir size: " << eodr.getRecordSize() << std::endl
              << "dir offs: " << eodr.getRecordOffset() << std::endl
              << "brdr off: " << borderOffset << std::endl
              << "last off: " << lastOffset << std::endl
              << "size    : " << eodr.getRecordsNumber() << std::endl
              << "eodr    : " << sizeof(eodr) << std::endl;
    #endif

    if (borderOffset > lastOffset) {
        #if ZPACK_DEBUG
        std::cout << "Writing new directory detects less file size, was: " << borderOffset << " become: " << lastOffset
                  << std::endl;
        #endif
        borderOffset = lastOffset;
        return (ullint) lastOffset;
    }

    borderOffset = lastOffset;

    int mb = 1024 * 1024;
    double border = 1.4;
    if (stats.archiveSize > (30 * mb)) {
        border = 1.1;
    } else if (stats.archiveSize > (10 * mb)) {
        border = 1.2;
    } else if (stats.archiveSize <= (10 * mb)) {
        border = 1.5;
    }

    if (borderOffset / stats.archiveSize > border) {
        repack();
    }

    return 0;
}

bool ZPack::good() {
    return file.good() && error_code == Errors::OK;
}

bool ZPack::fail() {
    return file.fail() || error_code != Errors::OK;
}

bool ZPack::bad() {
    return file.bad() || error_code != Errors::OK;
}
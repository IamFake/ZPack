#include <gtest/gtest.h>
#include "zpack.h"

namespace {
    TEST(General, CreateAndReadNewWithItem) {
        std::string tempFileName = tmpnam(NULL);
        std::string tempItemText = "AZZZAKAJSLKDNLAK SNDLK NSFLAKSNF ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknf";

        ZPack pack1;
        ZPack pack2;

        pack1.open(tempFileName.c_str(), true);
        pack1.packItem("special_item", tempItemText, "");
        pack1.write();
        pack1.close();

        auto stats1 = pack1.getStats();
        pack2.open(tempFileName.c_str());
        pack2.write();
        pack2.close();

        auto stats2 = pack2.getStats();

        remove(tempFileName.c_str());

        ASSERT_EQ(stats1.archiveSize, stats2.archiveSize);
    }

    TEST(General, CreateAndExtract) {
        std::string tempFileName = tmpnam(NULL);
        std::string tempItemText = "AZZZAKAJSLKDNLAK SNDLK NSFLAKSNF ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknfSNDLK NSFLAKSNF "
            "ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknfSNDLK NSFLAKSNF ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknfSNDLK NSFLAK"
            "SNF ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknf";

        ZPack pack1;

        pack1.open(tempFileName.c_str(), true);
        pack1.packItem("special_item", tempItemText, "");
        pack1.write();

        auto extractItem = pack1.extractStr("special_item");

        pack1.close();

        remove(tempFileName.c_str());

        ASSERT_EQ(extractItem.size(), tempItemText.size());
    }

    TEST(General, CreateDeleteAndRepack) {
        std::string tempFileName = tmpnam(NULL);
        std::string tempItemText = "AZZZAKAJSLKDNLAK SNDLK NSFLAKSNF ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknfSNDLK NSFLAKSNF "
            "ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknfSNDLK NSFLAKSNF ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknfSNDLK NSFLAK"
            "SNF ALKSFN ALKSFN ALKSFN LKFN ALSKNFALKSNFKsldknf";
        std::string tempItemText2 = "lkn asldknf aslknf owi34nfo3 4nr2o34nrt 2i3br2i sgdfi gsd98f ghs9d87f g6sd97ftg";

        ZPack pack1;
        std::cout << std::endl << "-------------------------------------" << std::endl;

        pack1.open(tempFileName.c_str(), true);
        pack1.packItem("special_item", tempItemText, "");
        pack1.packItem("special_item2", tempItemText2, "");
        pack1.write();

        auto stats = pack1.getStats();
        auto startOffset = stats.lastOffset;

        std::cout << "STATS 1:" << std::endl
                  << "dir offset  : " << stats.directoryOffset << std::endl
                  << "records     : " << stats.records << std::endl
                  << "lastOffset  : " << stats.lastOffset << std::endl
                  << "archiveSize : " << stats.archiveSize << std::endl
                  << "size Com    : " << stats.filesSizeCompressed << std::endl
                  << "size Unc    : " << stats.filesSizeUncompressed << std::endl << std::endl;

        pack1.remove("special_item2");
        pack1.write();

        stats = pack1.getStats();
        auto midOffset = stats.lastOffset;

        std::cout << "STATS 2:" << std::endl
                  << "dir offset  : " << stats.directoryOffset << std::endl
                  << "records     : " << stats.records << std::endl
                  << "lastOffset  : " << stats.lastOffset << std::endl
                  << "archiveSize : " << stats.archiveSize << std::endl
                  << "size Com    : " << stats.filesSizeCompressed << std::endl
                  << "size Unc    : " << stats.filesSizeUncompressed << std::endl << std::endl;

        ASSERT_LT(midOffset, startOffset);

        pack1.repack();

        stats = pack1.getStats();
        auto lastOffset = stats.lastOffset;

        std::cout << "STATS 3:" << std::endl
                  << "dir offset  : " << stats.directoryOffset << std::endl
                  << "records     : " << stats.records << std::endl
                  << "lastOffset  : " << stats.lastOffset << std::endl
                  << "archiveSize : " << stats.archiveSize << std::endl
                  << "size Com    : " << stats.filesSizeCompressed << std::endl
                  << "size Unc    : " << stats.filesSizeUncompressed << std::endl << std::endl;

        ASSERT_LT(lastOffset, midOffset);
        ASSERT_GT(lastOffset, 0);

        pack1.close();

        remove(tempFileName.c_str());
    }
}
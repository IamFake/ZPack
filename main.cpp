#include <cstdint>
#include <chrono>
#include <sstream>
#include <bitset>
#include "zpack.h"

int main(int argc, char **argv) {
    ZPack zp;
    std::cout << "ZPack version " << zp.version << std::endl;

    return 0;
}
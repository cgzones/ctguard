#include "../external/sha2/sha2.h"
#include "../libs/scopeguard.hpp"
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

int main(int argc, const char ** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " filename\n";
        return EXIT_FAILURE;
    }

    char result[SHA256_DIGEST_STRING_LENGTH];
    const size_t buff_size = 16384;
    unsigned char buffer[buff_size];

    const int fd = ::open(argv[1], O_RDONLY);
    if (fd == -1) {
        std::cerr << "Can not open file '" << argv[1] << "'\n";
        return EXIT_FAILURE;
    }

    ctguard::libs::scope_guard sg{ [fd]() { ::close(fd); } };

    ssize_t ret;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    while ((ret = ::read(fd, buffer, buff_size)) > 0) {
        SHA256_Update(&ctx, buffer, static_cast<size_t>(ret));
    }

    if (ret == -1) {
        std::cerr << "Can not read from file '" << argv[1] << "'\n";
        return EXIT_FAILURE;
    }

    SHA256_End(&ctx, result);

    std::cout << "sha256 sum: " << result << "\n";

    return EXIT_SUCCESS;
}

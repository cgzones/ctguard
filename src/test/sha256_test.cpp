#include <fcntl.h>
#include <sha2.h>
#include <unistd.h>

#include <array>
#include <iostream>

#include "../libs/scopeguard.hpp"

int main(int argc, const char ** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " filename\n";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return EXIT_FAILURE;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const int fd = ::open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        std::cerr << "Can not open file '" << argv[1] << "'\n";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return EXIT_FAILURE;
    }

    ctguard::libs::scope_guard sg{ [fd]() { ::close(fd); } };

    ssize_t ret;  // NOLINT(cppcoreguidelines-init-variables)
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    std::array<unsigned char, 16384> buffer;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)

    while ((ret = ::read(fd, buffer.data(), buffer.size())) > 0) {
        SHA256_Update(&ctx, buffer.data(), static_cast<size_t>(ret));
    }

    if (ret == -1) {
        std::cerr << "Can not read from file '" << argv[1] << "'\n";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return EXIT_FAILURE;
    }

    std::array<char, SHA256_DIGEST_STRING_LENGTH> result;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)

    SHA256_End(&ctx, result.data());

    std::cout << "sha256 sum: " << result.data() << "\n";

    return EXIT_SUCCESS;
}

#include <cxxtest/TestSuite.h>

#include "../external/sha2/sha2.h"

class sha256_testclass : public CxxTest::TestSuite {
public:
    void test_hash(void)
    {
        const char *input = "testString";
        const char *hash = "4acf0b39d9c4766709a3689f553ac01ab550545ffa4544dfc0b2cea82fba02a3";
        char result[SHA256_DIGEST_STRING_LENGTH];

        SHA256_CTX ctx;
        SHA256_Init(&ctx);

        SHA256_Update(&ctx, (const unsigned char *)input, strlen(input));

        SHA256_End(&ctx, result);

        TS_ASSERT_SAME_DATA(result, hash, (unsigned int)strlen(hash));
    }
};

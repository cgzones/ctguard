#include "../libs/logger.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

static void produce(int id)
{
    for (int i = 0; i < 100000; ++i) {
        FILE_LOG(ctguard::libs::log_level::DEBUG) << "test123"
                                                  << "456"
                                                  << "789" << ' ' << id;

        usleep(static_cast<unsigned>(static_cast<unsigned long>(random()) % 100));
    }
}

int main(int, char **)
{
    ctguard::libs::Output2FILE::stream() = fopen("log.txt", "a");

    std::string username{ "unknown" };
    std::cout << "start 1\n" << std::flush;

    FILE_LOG(ctguard::libs::log_level::INFO) << "Hello @info " << username;
    FILE_LOG(ctguard::libs::log_level::ERROR) << "Hello @error " << username;
    FILE_LOG(ctguard::libs::log_level::WARNING) << "Hello @warning " << username;
    FILE_LOG(ctguard::libs::log_level::DEBUG) << "Hello @debug " << username;
    FILE_LOG(ctguard::libs::log_level::DEBUG4) << "Hello @debug4 " << username;

    std::cout << "end 1\n" << std::flush;

    std::cout << "start 2\n" << std::flush;

    ctguard::libs::FILELog::domain() = "debugdomain";
    ctguard::libs::FILELog::reporting_level() = ctguard::libs::log_level::DEBUG4;

    FILE_LOG(ctguard::libs::log_level::INFO) << "Hello @info " << username;
    FILE_LOG(ctguard::libs::log_level::ERROR) << "Hello @error " << username;
    FILE_LOG(ctguard::libs::log_level::WARNING) << "Hello @warning " << username;
    FILE_LOG(ctguard::libs::log_level::DEBUG) << "Hello @debug " << username;
    FILE_LOG(ctguard::libs::log_level::DEBUG4) << "Hello @debug4 " << username;

    std::cout << "end 2\n" << std::flush;

    std::cout << "start 3\n" << std::flush;

    auto p1 = std::thread(produce, 1);
    auto p2 = std::thread(produce, 2);
    auto p3 = std::thread(produce, 3);

    std::cout << "running 3\n" << std::flush;

    p1.join();
    p2.join();
    p3.join();
    std::cout << "end 3\n" << std::flush;

    return 0;
}

#include "asio.hpp"
#include "proxy/proxy.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "common/configure.h"
#include <iostream>

int main(int argc, const char* argv[]) {
    try {
        if(argc != 2) {
          std::cerr << "should have one arg : conf file." << std::endl;
          exit(-1);
        }
        //conf init.
        Configure::instance().load(argv[1]);

        //log init.
        auto console = spdlog::stdout_logger_st("console");
        console->set_level(spdlog::level::trace);

        //net init.
        asio::io_context io_context;

        //start proxy first.
        Proxy proxy(io_context, 12345, 12346, console);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cout << "exception : " << e.what() << std::endl;
    }
}

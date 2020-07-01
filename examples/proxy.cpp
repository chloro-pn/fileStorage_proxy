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
        console->set_pattern("[%@] <%!> {%c} |%l| %v");
        console->set_level(spdlog::level::trace);

        //net init.
        asio::io_context io_context;

        //start proxy first.
        uint16_t p1 = Configure::instance().get<uint16_t>("client_port");
        uint16_t p2 = Configure::instance().get<uint16_t>("storage_server_port");
        Proxy proxy(io_context, p1, p2, console);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cout << "exception : " << e.what() << std::endl;
    }
}

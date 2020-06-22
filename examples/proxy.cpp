#include "asio.hpp"
#include "proxy/proxy.h"
#include "spdlog/sinks/stdout_sinks.h"
#include <iostream>

int main() {
    try {
        auto console = spdlog::stdout_logger_st("console");
        console->set_level(spdlog::level::trace);
        asio::io_context io_context;
        //start proxy first.
        Proxy proxy(io_context, 12345, 12346, console);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cout << "exception : " << e.what() << std::endl;
    }
}

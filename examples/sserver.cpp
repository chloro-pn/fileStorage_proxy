#include "asio.hpp"
#include "sserver/storage_server.h"
#include "spdlog/sinks/stdout_sinks.h"
#include <iostream>

int main() {
    try {
        auto console = spdlog::stdout_logger_st("console");
        console->set_level(spdlog::level::trace);
        asio::io_context io_context;
        //start sserver and connect to proxy.
        StorageServer ss(io_context, "127.0.0.1", "12346", console);
        ss.connectToProxyServer();
        io_context.run();
    }
    catch (std::exception& e) {
        std::cout << "exception : " << e.what() << std::endl;
    }
}

#include "asio.hpp"
#include "proxy/proxy.h"
#include "sserver/storage_server.h"
#include "client/agent.h"
#include "spdlog/sinks/stdout_sinks.h"
#include <iostream>
#include <string>
#include <ctime>

int main() {
    try {
        auto console = spdlog::stdout_logger_st("console");
        console->set_level(spdlog::level::trace);
        asio::io_context io_context;
        //start proxy first.
        Proxy proxy(io_context, 12345, 12346, console);
        //start sserver and connect to proxy.
        //StorageServer ss(io_context, "127.0.0.1", "12346", console);
        //ss.connectToProxyServer();
        //start agent.
        //Agent agent(io_context, "127.0.0.1", "12345", console);
        //agent.uploadFile("/home/pn/asio/asio/client.cpp");
        io_context.run();
    }
    catch (std::exception& e) {
        std::cout << "exception : " << e.what() << std::endl;
    }
}

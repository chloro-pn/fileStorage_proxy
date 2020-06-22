#include "asio.hpp"
#include "client/agent.h"
#include "spdlog/sinks/stdout_sinks.h"
#include <iostream>

int main() {
    try {
        auto console = spdlog::stdout_logger_st("console");
        console->set_level(spdlog::level::trace);
        asio::io_context io_context;
        //start agent.
        Agent agent(io_context, "127.0.0.1", "12345", console);
        agent.uploadFile("/home/pn/asio/asio/client.cpp");
        io_context.run();
    }
    catch (std::exception& e) {
        std::cout << "exception : " << e.what() << std::endl;
    }
}

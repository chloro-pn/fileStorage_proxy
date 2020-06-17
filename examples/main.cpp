#include "asio.hpp"
#include "proxy/proxy.h"
#include "sserver/storage_server.h"
#include <iostream>
#include <string>
#include <ctime>

int main() {
    try {
        asio::io_context io_context;
        //start proxy first.
        Proxy proxy(io_context, 12345, 12346);
        //start sserver and connect to proxy.
        StorageServer ss(io_context, "127.0.0.1", "12346");
        ss.connectToProxyServer();
        io_context.run();
    }
    catch (std::exception& e) {
        std::cout << "exception : " << e.what() << std::endl;
    }
}

#include "server.h"

int main()
{
    try {
        boost::asio::io_service io_service;               // io_service handles asynchronous I/O requests
        Server                  server{io_service, 8080}; // create server
        server.run();                                     // run server
    } catch (std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
    return 0;
}
#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <thread>

using boost::asio::ip::tcp;

class Client
{
  public:
    Client(const std::string& host, const std::string& port); // Сonstructor that takes in a host and port as parameters
    void run();                                               // This method runs the client and handles the message input and output

  private:
    boost::asio::io_service m_io_service; // io_service handles asynchronous I/O requests
    tcp::resolver           m_resolver;   // resolver resolves a host name and port number to an endpoint
    tcp::resolver::iterator m_endpoint;   // endpoint iterator to iterate over all possible endpoints
    tcp::socket             m_socket;     // socket object used to read and write data

    void readMessage();                              // method reads a message from the socket
    void processInput();                             // method processes the input from the user
    void processMessage(const std::string& command); // method processes the message received from the server
    void initializeUser();                           // method registers user
};

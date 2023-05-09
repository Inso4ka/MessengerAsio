#pragma once 

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <vector>
#include <map>
#include <thread>
#include <memory>
#include <sqlite3.h>
#include <boost/random.hpp>

using boost::asio::ip::tcp;

class Server
{
  public:
    Server(boost::asio::io_service& io_service, int port); // сonstructor that takes in a asynchronous I/O requests and port as parameters
    void run();                                            // this method runs the server and handles the connection

  private:
    std::map<std::string, std::string>  m_data;
    tcp::acceptor                       m_acceptor;                                                              // creates sockets for data exchange
    std::map<std::string, tcp::socket*> m_clients;                                                               // map for socket storage
    void                                sendMessage(const std::string& message, const std::string& recipientId); // send message
    void                                handleConnection(std::shared_ptr<tcp::socket> socketPtr);                // function to process the connection
};

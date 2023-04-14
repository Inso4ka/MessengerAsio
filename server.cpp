#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <vector>
#include <map>
#include <thread>
#include <memory>

using boost::asio::ip::tcp;

std::map<std::string, tcp::socket*> clientSockets; // map for socket storage
std::vector<std::thread>            threads;       // vector for thread storage

void sendMessage(const std::string& message, const std::string& recipientId)
{
    std::cout << "Sending message \"" << message << "\" to " << recipientId << std::endl;
    auto it = clientSockets.find(recipientId); // iterator to a specific identifier

    if (it != clientSockets.end() && it->second->is_open()) // if the socket exists and is open
    {
        //  send message through socket
        boost::asio::write(*it->second, boost::asio::buffer(message));
    } else // else throwing an exception
    {
        std::cerr << "Recipient " << recipientId << " not found!" << std::endl;
        clientSockets.erase(it); // delete map element
    }
}

void handleConnection(std::shared_ptr<tcp::socket> socketPtr)
{
    std::string clientId = socketPtr->remote_endpoint().address().to_string(); // getting ID client based on IP

    if (!socketPtr->is_open()) {
        std::cerr << "Socket for client " << clientId << " is not open." << std::endl;
        return;
    }

    std::cout << "New connection from " << clientId << std::endl;

    clientSockets[clientId] = socketPtr.get(); // adding client's socket to map

    while (true) {
        try {
            // readind client's message
            char                      buf[1024];
            boost::system::error_code error;
            size_t                    len = socketPtr->read_some(boost::asio::buffer(buf), error);

            if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset) {
                std::cout << "Client " << clientId << " disconnected." << std::endl;
                break; // abort if the user disconnected
            } else if (error) {
                std::cerr << "Error reading from client " << clientId << ": " << error.message() << std::endl;
                break; // abort if we got an error
            }

            std::istringstream iss(std::string(buf, len));
            std::string        command;
            iss >> command;

            if (command == "send") // if command send - send message to specific member
            {
                std::string recipientId;
                iss.ignore();
                std::getline(iss, recipientId, ' '); // reading recipient's ID

                tcp::socket* recipientSocket = clientSockets[recipientId];
                if (recipientSocket) // if recipient exists in map, send message
                {
                    std::string message = clientId + ": " + iss.str().substr(5 + recipientId.length()); // create new message
                    sendMessage(message, recipientId);
                } else // if recipient wasn't found, throw an error
                {
                    std::string errorMessage = "Error: client '" + recipientId + "' not found.";
                    boost::asio::write(*socketPtr, boost::asio::buffer(errorMessage));
                }
            }
            if (command == "online") { // if command online - show map content to client
                for (const auto& [value, key] : clientSockets) {
                    boost::asio::write(*socketPtr, boost::asio::buffer("\nIP: " + value));
                }
            } else { // if the command is not in the registry
                std::cerr << "Unknown command: " << command << std::endl;
            }

        } catch (std::exception& e) {
            std::cerr << "Exception caught in handleConnection(): " << e.what() << std::endl;
        }
    }

    // deleting socket from map
    clientSockets.erase(clientId);
}

int main(int argc, char* argv[])
{
    try {
        // argv for port
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
            return 1;
        }

        // main part IO
        boost::asio::io_service io_service;
        tcp::acceptor           acceptor(io_service, tcp::endpoint(tcp::v4(), std::atoi(argv[1])));

        std::cout << "Server started listening on " << std::atoi(argv[1]) << std::endl;

        for (;;) {
            // socket for new connection (used shared_ptr cause if we're using std::thread, socket is temporary object, it will be destroyed
            // prematurely)
            std::shared_ptr<tcp::socket> socketPtr = std::make_shared<tcp::socket>(io_service);

            // waiting for new connections
            acceptor.accept(*socketPtr);

            // run the handleConnection() function in a new thread to process the connection
            std::thread th(handleConnection, socketPtr);
            th.detach();

            threads.emplace_back(std::move(th)); // add new thread to vector
        }
    } catch (std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }

    for (auto& t : threads) // waiting for all threads to finish
    {
        t.join();
    }

    return 0;
}
#include <iostream>
#include <boost/asio.hpp>
#include <thread>

using boost::asio::ip::tcp;

void readMessage(tcp::socket& socket)
{
    try {
        while (true) {
            // reading message from client
            char                      buf[1024];
            boost::system::error_code error;
            size_t                    len = socket.read_some(boost::asio::buffer(buf), error);

            if (error == boost::asio::error::eof) // if disconnected
            {
                std::cout << "Server disconnected." << std::endl;
                break; // abort if the user disconnected
            } else if (error) {
                std::cerr << "Error reading from server: " << error.message() << std::endl;
                break; // abort if we got an error
            }
            std::cout << "Received message: " << std::string(buf, len) << std::endl; // display message
        }
    } catch (std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[])
{
    try {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
            return 1;
        }
        // main part IO
        boost::asio::io_service io_service;
        tcp::resolver           resolver(io_service);
        tcp::resolver::query    query(argv[1], argv[2]);
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

        tcp::socket socket(io_service);
        boost::asio::connect(socket, endpoint_iterator);

        std::cout << "Connected to " << argv[1] << ":" << argv[2] << std::endl;

        std::thread t([&socket] { readMessage(socket); }); // new thread for messages from server

        while (true) {
            try {
                std::string command; // reading command from user
                std::getline(std::cin, command);

                // if command is send
                if (command.substr(0, 4) == "send") {
                    std::string recipientId;
                    std::string message;
                    size_t      spacePosition = command.find_first_of(" ");
                    if (spacePosition != std::string::npos) {
                        recipientId                 = command.substr(spacePosition + 1);
                        size_t messageStartPosition = recipientId.find_first_of(" ") + 1;
                        message                     = recipientId.substr(messageStartPosition);
                        recipientId                 = recipientId.substr(0, messageStartPosition - 1);

                        // sending message through socket
                        std::string fullMessage = "send " + recipientId + " " + message;
                        boost::asio::write(socket, boost::asio::buffer(fullMessage));
                    } else {
                        std::cerr << "Invalid send command format." << std::endl;
                    }
                } else if (command.substr(0, 5) == "online") {
                    boost::asio::streambuf receive_buffer;
                    boost::asio::read(socket, receive_buffer, boost::asio::transfer_all());
                } else {
                    // sending message through socket to server
                    boost::asio::write(socket, boost::asio::buffer(command + "\n"));
                }
            } catch (std::exception& e) {
                std::cerr << "Exception caught: " << e.what() << std::endl;
                break;
            }
        }

        t.join();       // close thread
        socket.close(); // close socket
    } catch (std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }

    return 0;
}

#include "server.h"
#include "database.h"

Server::Server(boost::asio::io_service& io_service, int port) : m_acceptor(io_service, tcp::endpoint(tcp::v4(), port))
{
    std::cout << "Server started listening on " << port << std::endl;
}

void Server::run()
{
    while (true) {
        std::shared_ptr<tcp::socket> socketPtr = std::make_shared<tcp::socket>(m_acceptor.get_executor());
        m_acceptor.accept(*socketPtr);
        std::thread workerThread(&Server::handleConnection, this, std::move(socketPtr));
        workerThread.join();
    }
}

void Server::sendMessage(const std::string& message, const std::string& recipientId)
{
    std::cout << "Sending message: \"" << message << "\" to " << recipientId << std::endl;
    auto it = m_clients.find(recipientId); // iterator to a specific identifier

    if (it != m_clients.end() && it->second->is_open()) // if the socket exists and is open
    {
        boost::asio::write(*it->second, boost::asio::buffer(message)); // send message through socket
    } else                                                             // else throwing an exception
    {
        std::cerr << "Recipient " << recipientId << " not found!" << std::endl;
        m_clients.erase(it); // delete map element
    }
}

void Server::handleConnection(std::shared_ptr<tcp::socket> socketPtr)
{
    std::string login, password;
    std::string clientId = socketPtr->remote_endpoint().address().to_string(); // getting ID client based on IP
    if (!socketPtr->is_open()) {
        std::cerr << "Socket for client " << clientId << " is not open." << std::endl;
        return;
    }
    std::cout << "New connection from " << clientId << std::endl;
    try {
        // readind client's message
        char                      buf[1024];
        boost::system::error_code error;
        size_t                    len = socketPtr->read_some(boost::asio::buffer(buf), error);
        if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset) {
            std::cout << "Client " << clientId << " disconnected." << std::endl;
            return; // abort if the user disconnected
        } else if (error) {
            std::cerr << "Error reading from client " << clientId << ": " << error.message() << std::endl;
            return; // abort if we got an error
        }
        {
            // processing registration
            std::istringstream iss(std::string(buf, len));
            iss >> login >> password;
        }

        Database user(login, password, socketPtr);
        user.getIntoSystem();
        if (password == "-") {
            boost::asio::write(*socketPtr, boost::asio::buffer("\nDon't forget to change the password using the \"setpassword\" command.\n "));
        }
        m_clients[login] = socketPtr.get();

        // processing messages
        while (true) {
            len = socketPtr->read_some(boost::asio::buffer(buf), error); // read user input

            if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset) {
                std::cout << "Client " << clientId << " disconnected." << std::endl;
                break; // client disconnected, break out of the loop
            } else if (error) {
                std::cerr << "Error reading from client " << clientId << ": " << error.message() << std::endl;
                break; // error occurred, break out of the loop
            }

            // process user input
            std::istringstream iss(std::string(buf, len));
            std::string        command;
            iss >> command;

            if (command == "send") // if command send - send message to specific member
            {
                std::string recipientId;
                iss.ignore();
                std::getline(iss, recipientId, ' '); // reading recipient's ID

                tcp::socket* recipientSocket = m_clients[recipientId];
                if (recipientSocket) // if recipient exists in map, send message
                {
                    std::string message = "~" + login + ": " + iss.str().substr(5 + recipientId.length()); // create new message
                    sendMessage(message, recipientId);
                } else // if recipient wasn't found, throw an error
                {
                    std::string errorMessage = "Error: client '" + recipientId + "' not found.";
                    boost::asio::write(*socketPtr, boost::asio::buffer(errorMessage));
                }
            } else if (command == "online") { // if command online - show map content to client
                for (const auto& [value, key] : m_clients) {
                    boost::asio::write(*socketPtr, boost::asio::buffer("\nIP: " + clientId));
                }
            } else if (command == "setpassword") {
                std::string oldpassword, newpassword;
                iss.ignore();
                iss >> oldpassword >> newpassword;
                if (newpassword.length() < 8) {
                    boost::asio::write(*socketPtr, boost::asio::buffer("Error: the password must be at least 8 characters long."));
                } else if (newpassword.find(' ') != std::string::npos) {
                    boost::asio::write(*socketPtr, boost::asio::buffer("Error: the password must not contain spaces."));
                } else {
                    user.changePassword(oldpassword, newpassword);
                }
            } else if (command == "add") {
                std::string newusername;
                iss.ignore();
                iss >> newusername;
                bool hasNonAlphaNumCharLog = false;
                for (char ch : newusername) {
                    if (!isalpha(ch) && !isdigit(ch)) {
                        hasNonAlphaNumCharLog = true;
                        break;
                    }
                }
                if (hasNonAlphaNumCharLog) {
                    std::cout << "Error: the login must contain only English letters and digits."
                              << "\n";
                    break;
                } else {
                    user.addUser(newusername);
                }
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Exception in thread: " << e.what() << std::endl;
    }

    m_clients.erase(login); // remove client's socket from map
}
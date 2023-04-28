#include "client.h"

Client::Client(const std::string& host, const std::string& port)
    : m_io_service(), m_resolver(m_io_service), m_endpoint(m_resolver.resolve(tcp::resolver::query(host, port))), m_socket(m_io_service)
{
    if (m_endpoint != tcp::resolver::iterator()) {
        std::cout << "Connected to " << m_endpoint->host_name() << ":" << m_endpoint->service_name() << std::endl;
    }
}

void Client::run()
{
    try {
        boost::asio::connect(m_socket, m_endpoint);
        std::thread t([&]() { readMessage(); });
        initializeUser();
        processInput();
        t.join();
        m_socket.close();
    } catch (std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

void Client::initializeUser()
{
    char symbol{};
    std::cout << "Are you already registered? [Y/N]: ";
    std::cin >> symbol;
    if (symbol == 'Y') {
        std::string username, password;
        std::cout << "Enter username: ";
        std::cin >> username;
        std::cout << "Enter password: ";
        std::cin >> password;

        std::string buffer{};
        boost::asio::write(m_socket, boost::asio::buffer(username + " " + password));
        boost::asio::read(m_socket, boost::asio::buffer(buffer));
    } else {
        std::cout << "To get access to the chat, you need to contact with administrator.\n";
    }
}

void Client::readMessage()
{
    try {
        while (true) {
            // reading message from client
            char                      buf[1024];
            boost::system::error_code error;
            size_t                    len = m_socket.read_some(boost::asio::buffer(buf), error);

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

void Client::processInput()
{
    while (true) {
        try {
            std::string command; 
            std::getline(std::cin, command);

            if (command.substr(0, 4) == "send") {
                processMessage(command); 
            } else if (command.substr(0, 11) == "setpassword") {
                processMessage(command);
            } else if (command.substr(0, 3) == "add") {
                std::string username;
                size_t      spacePosition = command.find_first_of(" ");
                if (spacePosition != std::string::npos) {
                    username                = command.substr(spacePosition + 1);
                    std::string fullMessage = "add " + username + " ";
                    boost::asio::write(m_socket, boost::asio::buffer(fullMessage));
                }
            }
        } catch (std::exception& e) {
            std::cerr << "Exception caught: " << e.what() << std::endl;
            break;
        }
    }
}

void Client::processMessage(const std::string& command)
{
    // analysis of the received message
    std::string recipientId;
    std::string message;
    size_t      spacePosition = command.find_first_of(" ");
    if (spacePosition != std::string::npos) {
        recipientId                 = command.substr(spacePosition + 1);
        size_t messageStartPosition = recipientId.find_first_of(" ") + 1;
        message                     = recipientId.substr(messageStartPosition);
        recipientId                 = recipientId.substr(0, messageStartPosition - 1);
        if (command.substr(0, 4) == "send") {
            std::string fullMessage = "send " + recipientId + " " + message;
            boost::asio::write(m_socket, boost::asio::buffer(fullMessage));
        } else {
            std::string fullMessage = "setpassword " + recipientId + " " + message;
            boost::asio::write(m_socket, boost::asio::buffer(fullMessage));
        }
    } else {
        std::cerr << "Invalid send command format." << std::endl;
    }
}

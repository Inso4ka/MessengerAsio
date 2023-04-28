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

class Database
{
  public:
    Database(const std::string& username, const std::string& password, std::shared_ptr<tcp::socket> socketPtr)
        : m_username(username), m_password(password), m_socket(socketPtr)
    {
        int res = sqlite3_open("database.db", &m_db);
        if (res != SQLITE_OK) {
            boost::asio::write(*m_socket, boost::asio::buffer("Error opening database."));
            return;
        }
        create();
    }

    std::string generateRandomPassword()
    {
        const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        const int         length  = 8;

        boost::random::mt19937                    rng(std::time(nullptr));
        boost::random::uniform_int_distribution<> index_dist(0, charset.size() - 1);

        std::string password;
        for (int i = 0; i < length; i++) {
            password += charset[index_dist(rng)];
        }

        return password;
    }

    void getIntoSystem()
    {
        std::string   stmt = "SELECT * FROM users WHERE username='" + m_username + "'";
        sqlite3_stmt* sql_stmt;
        int           res = sqlite3_prepare_v2(m_db, stmt.c_str(), -1, &sql_stmt, nullptr);
        if (res != SQLITE_OK) {
            boost::asio::write(*m_socket, boost::asio::buffer("Error preparing SQL statement."));
            sqlite3_finalize(sql_stmt);
            return;
        }
        res = sqlite3_step(sql_stmt);
        if (res == SQLITE_ROW) {
            std::string stored_password((const char*)sqlite3_column_text(sql_stmt, 1));
            if (stored_password == m_password) {
                boost::asio::write(*m_socket, boost::asio::buffer("Welcome back, " + m_username + "!\n"));
                boost::asio::write(*m_socket, boost::asio::buffer("Login successful."));
            } else {
                boost::asio::write(*m_socket, boost::asio::buffer("Invalid password."));
                // Разрываем соединение при неправильном пароле
                try {
                    m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                    m_socket->close();
                } catch (std::exception& e) {
                    std::cerr << "Error closing socket: " << e.what() << std::endl;
                }
            }
        } else {
            boost::asio::write(*m_socket, boost::asio::buffer("User not found."));
            try {
                m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                m_socket->close();
            } catch (std::exception& e) {
                std::cerr << "Error closing socket: " << e.what() << std::endl;
            }
        }
        sqlite3_finalize(sql_stmt);
    }
    void addUser(const std::string& username)
    {
        m_password       = generateRandomPassword();
        std::string stmt = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + m_password + "')";
        sqlite3_stmt* sql_stmt;
        int           res = sqlite3_prepare_v2(m_db, stmt.c_str(), -1, &sql_stmt, nullptr);
        if (res != SQLITE_OK) {
            boost::asio::write(*m_socket, boost::asio::buffer("Error preparing SQL statement."));
            sqlite3_finalize(sql_stmt);
            return;
        }

        res = sqlite3_step(sql_stmt);
        if (res != SQLITE_DONE) {
            boost::asio::write(*m_socket, boost::asio::buffer("Error adding user to the database."));
        } else {
            boost::asio::write(*m_socket, boost::asio::buffer("User " + username + " has been added to the database."));
        }

        sqlite3_finalize(sql_stmt);
    }

    void changePassword(const std::string& oldPassword, const std::string& newPassword)
    {
        std::string   stmt = "SELECT * FROM users WHERE username='" + m_username + "'";
        sqlite3_stmt* sql_stmt;
        int           res = sqlite3_prepare_v2(m_db, stmt.c_str(), -1, &sql_stmt, nullptr);
        if (res != SQLITE_OK) {
            boost::asio::write(*m_socket, boost::asio::buffer("Error preparing SQL statement."));
            sqlite3_finalize(sql_stmt);
            return;
        }
        res = sqlite3_step(sql_stmt);
        if (res != SQLITE_ROW) {
            boost::asio::write(*m_socket, boost::asio::buffer("User not found."));
            sqlite3_finalize(sql_stmt);
            return;
        }
        std::string stored_password((const char*)sqlite3_column_text(sql_stmt, 1));
        if (stored_password != oldPassword) {
            boost::asio::write(*m_socket, boost::asio::buffer("Old password is incorrect."));
            sqlite3_finalize(sql_stmt);
            return;
        }
        sqlite3_finalize(sql_stmt);
        stmt = "UPDATE users SET password=? WHERE username=?";
        res  = sqlite3_prepare_v2(m_db, stmt.c_str(), -1, &sql_stmt, nullptr);
        if (res != SQLITE_OK) {
            boost::asio::write(*m_socket, boost::asio::buffer("Error preparing SQL statement."));
            sqlite3_finalize(sql_stmt);
            return;
        }
        sqlite3_bind_text(sql_stmt, 1, newPassword.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(sql_stmt, 2, m_username.c_str(), -1, SQLITE_TRANSIENT);
        res = sqlite3_step(sql_stmt);
        if (res == SQLITE_DONE) {
            boost::asio::write(*m_socket, boost::asio::buffer("Password changed successfully."));
        } else {
            boost::asio::write(*m_socket, boost::asio::buffer("Error changing password."));
        }
        sqlite3_finalize(sql_stmt);
    }

    ~Database()
    {
        sqlite3_close(m_db);
    }

  private:
    sqlite3*                     m_db;
    std::string                  m_username;
    std::string                  m_password;
    std::shared_ptr<tcp::socket> m_socket;

    void create()
    {
        int res = sql_exec(m_db, "CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password TEXT)");
        if (res != SQLITE_OK) {
            boost::asio::write(*m_socket, boost::asio::buffer("Error creating table.\n"));
            return;
        }
    }
    static int sql_exec(sqlite3* db, const char* sql_stmt)
    {
        char* errmsg = nullptr;
        int   res    = sqlite3_exec(db, sql_stmt, nullptr, nullptr, &errmsg);
        if (res != SQLITE_OK) {
            std::cerr << "Error executing SQL statement: " + std::string(errmsg) + "\n";
            sqlite3_free(errmsg);
        }
        return res;
    }
};

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
            boost::asio::write(
                *socketPtr,
                boost::asio::buffer("\nDon't forget to change the password using the \"setpassword\" command.\n Your current password is \"-\"."));
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
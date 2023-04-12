#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <vector>
#include <map>
#include <thread>
#include <memory>

using boost::asio::ip::tcp;

std::map<std::string, tcp::socket*> clientSockets; // map для хранения идентификаторов клиентов и соответствующих им сокетов
std::vector<std::thread> threads; // вектор для хранения потоков

void sendMessage(const std::string& message, const std::string& recipientId)
{
    std::cout << "Sending message \"" << message << "\" to " << recipientId << std::endl;

    auto it = clientSockets.find(recipientId); // получаем итератор на элемент с указанным идентификатором

    if (it != clientSockets.end() && it->second->is_open()) // если сокет получателя найден и открыт
    {
        // отправляем сообщение через сокет
        boost::asio::write(*it->second, boost::asio::buffer(message));
    }
    else // иначе выдаем ошибку
    {
        std::cerr << "Recipient " << recipientId << " not found!" << std::endl;
        clientSockets.erase(it); // удаляем запись из map
    }
}


void handleConnection(std::shared_ptr<tcp::socket> socketPtr)
{
    std::string clientId = socketPtr->remote_endpoint().address().to_string(); // получаем идентификатор клиента на основе его IP-адреса и порта

    if (!socketPtr->is_open()) {
        std::cerr << "Socket for client " << clientId << " is not open." << std::endl;
        return;
    }

    std::cout << "New connection from " << clientId << std::endl;

    clientSockets[clientId] = socketPtr.get(); // добавляем сокет клиента в map

    while (true)
    {
        try
        {
            // читаем сообщение от клиента
            char buf[1024];
            boost::system::error_code error;
            size_t len = socketPtr->read_some(boost::asio::buffer(buf), error);

            if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset) {
                std::cout << "Client " << clientId << " disconnected." << std::endl;
                break; // выходим из цикла чтения сообщений от клиента
            }
            else if (error) {
                std::cerr << "Error reading from client " << clientId << ": " << error.message() << std::endl;
                break; // выходим из цикла чтения сообщений от клиента
            }

            // разбираем сообщение, чтобы понять, что нужно сделать с сообщением (отправить его получателю или нет)
            std::istringstream iss(std::string(buf, len));
            std::string command;
            iss >> command;

            // если клиент отправляет сообщение конкретному получателю
            if (command == "send")
            {
                std::string recipientId;
                iss.ignore();
                std::getline(iss, recipientId, ' '); // считываем идентификатор получателя

                tcp::socket* recipientSocket = clientSockets[recipientId];
                if (recipientSocket) // если получатель найден, отправляем ему сообщение
                {
                    std::string message = clientId + ": " + iss.str().substr(5 + recipientId.length()); // формируем новое сообщение с идентификатором отправителя
                    sendMessage(message, recipientId);
                } 
                else // если получатель не найден, отправляем сообщение об ошибке
                {
                    std::string errorMessage = "Error: client '" + recipientId + "' not found.";
                    boost::asio::write(*socketPtr, boost::asio::buffer(errorMessage));
                }
            }
            if (command == "online") {
                for (const auto& [value, key] : clientSockets) {
                    boost::asio::write(*socketPtr, boost::asio::buffer("\nIP: " + value));
                }
            }
            // если клиент отправляет общее сообщение
            else
            {
                std::cerr << "Unknown command: " << command << std::endl;
            }

        }
        catch (std::exception& e)
        {
            std::cerr << "Exception caught in handleConnection(): " << e.what() << std::endl;
        }
    }

    // удаляем сокет клиента из map
    clientSockets.erase(clientId);
}


int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
            return 1;
        }

        boost::asio::io_service io_service;
        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), std::atoi(argv[1])));

        std::cout << "Server started listening on " << std::atoi(argv[1]) << std::endl;

        for (;;)
        {
            // создаем сокет для нового подключения
            std::shared_ptr<tcp::socket> socketPtr = std::make_shared<tcp::socket>(io_service);

            // ждем новых подключений
            acceptor.accept(*socketPtr);

            // запускаем функцию handleConnection() в новом потоке для обработки подключения
            std::thread th(handleConnection, socketPtr);
            th.detach();


            threads.emplace_back(std::move(th)); // добавляем поток в вектор потоков
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }

    for (auto& t : threads) // ожидаем завершения всех потоков
    {
        t.join();
    }

    return 0;
}
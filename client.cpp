#include <iostream>
#include <boost/asio.hpp>
#include <thread>

using boost::asio::ip::tcp;

void readMessage(tcp::socket& socket)
{
    try
    {
        while (true)
        {
            // читаем сообщение от сервера
            char buf[1024];
            boost::system::error_code error;
            size_t len = socket.read_some(boost::asio::buffer(buf), error);

            if (error == boost::asio::error::eof) // обрабатываем случай отсоединения от сервера
            {
                std::cout << "Server disconnected." << std::endl;
                break; // выходим из цикла чтения сообщений
            }
            else if (error) // обрабатываем другие ошибки
            {
                std::cerr << "Error reading from server: " << error.message() << std::endl;
                break; // выходим из цикла чтения сообщений
            }

            // выводим сообщение на экран
            std::cout << "Received message: " << std::string(buf, len) << std::endl;
        }
    }
    catch (std::exception& e) // обрабатываем исключения
    {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
            return 1;
        }

        boost::asio::io_service io_service;
        tcp::resolver resolver(io_service);
        tcp::resolver::query query(argv[1], argv[2]);
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

        tcp::socket socket(io_service);
        boost::asio::connect(socket, endpoint_iterator);

        std::cout << "Connected to " << argv[1] << ":" << argv[2] << std::endl;

        std::thread t([&socket]{ readMessage(socket); }); // запускаем новый поток для чтения сообщений от сервера

       while (true)
        {
            try
            {
                // читаем введенную пользователем команду
                std::string command;
                std::getline(std::cin, command);

                // проверяем, является ли команда командой отправки сообщения
                if (command.substr(0, 4) == "send")
                {
                    // извлекаем идентификатор получателя и текст сообщения из команды
                    std::string recipientId;
                    std::string message;
                    size_t spacePosition = command.find_first_of(" ");
                    if (spacePosition != std::string::npos)
                    {
                        recipientId = command.substr(spacePosition + 1);
                        size_t messageStartPosition = recipientId.find_first_of(" ") + 1;
                        message = recipientId.substr(messageStartPosition);
                        recipientId = recipientId.substr(0, messageStartPosition - 1);

                        // отправляем сообщение указанному получателю через сокет
                        std::string fullMessage = "send " + recipientId + " " + message;
                        boost::asio::write(socket, boost::asio::buffer(fullMessage));
                    }
                    else
                    {
                        std::cerr << "Invalid send command format." << std::endl;
                    }
                }
                else if (command.substr(0, 5) == "online") {
                    boost::asio::streambuf receive_buffer;
                    boost::asio::read(socket, receive_buffer, boost::asio::transfer_all());
                }
                else
                {
                    // отправляем команду серверу через сокет
                    boost::asio::write(socket, boost::asio::buffer(command + "\n"));
                }
            }
            catch (std::exception& e) // обрабатываем исключения
            {
                std::cerr << "Exception caught: " << e.what() << std::endl;
                break; // выходим из цикла отправки команд
            }
        }

        // останавливаем поток чтения сообщений и закрываем сокет
        t.join();
        socket.close();
    }
    catch (std::exception& e) // обрабатываем исключения
    {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }

    return 0;
}

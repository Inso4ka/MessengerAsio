#include <boost/asio.hpp>
#include <sqlite3.h>
#include <boost/random.hpp>

using boost::asio::ip::tcp;

class Database
{
  public:
    Database(const std::string& username, const std::string& password, std::shared_ptr<tcp::socket> socketPtr);
    std::string generateRandomPassword();
    void        getIntoSystem();
    void        addUser(const std::string& username);
    void        changePassword(const std::string& oldPassword, const std::string& newPassword);
    ~Database();

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
#include "database.h"

Database::Database(const std::string& username, const std::string& password, std::shared_ptr<tcp::socket> socketPtr)
    : m_username(username), m_password(password), m_socket(socketPtr)
{
    int res = sqlite3_open("database.db", &m_db);
    if (res != SQLITE_OK) {
        boost::asio::write(*m_socket, boost::asio::buffer("Error opening database."));
        return;
    }
    create();
}

void Database::create()
{
    int res = sql_exec(m_db, "CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password TEXT)");
    if (res != SQLITE_OK) {
        boost::asio::write(*m_socket, boost::asio::buffer("Error creating table.\n"));
        return;
    }
}

int Database::sql_exec(sqlite3* db, const char* sql_stmt)
{
    char* errmsg = nullptr;
    int   res    = sqlite3_exec(db, sql_stmt, nullptr, nullptr, &errmsg);
    if (res != SQLITE_OK) {
        std::cerr << "Error executing SQL statement: " + std::string(errmsg) + "\n";
        sqlite3_free(errmsg);
    }
    return res;
}

std::string Database::generateRandomPassword()
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

void Database::getIntoSystem()
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
            std::cout << "ONLINE: " << m_username << "\n";
            boost::asio::write(*m_socket, boost::asio::buffer("Login successful."));
        } else {
            boost::asio::write(*m_socket, boost::asio::buffer("Invalid password."));
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

void Database::addUser(const std::string& username)
{
    m_password         = generateRandomPassword();
    std::string   stmt = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + m_password + "')";
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
        std::cout << "ADDED: " << username << "\n";
    }

    sqlite3_finalize(sql_stmt);
}

void Database::changePassword(const std::string& oldPassword, const std::string& newPassword)
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
        std::cout << "PASSWORD UPDATED: " << m_username << ":" << newPassword << "\n";
        boost::asio::write(*m_socket, boost::asio::buffer("Password changed successfully."));
    } else {
        boost::asio::write(*m_socket, boost::asio::buffer("Error changing password."));
    }
    sqlite3_finalize(sql_stmt);
}

Database::~Database()
{
    sqlite3_close(m_db);
}
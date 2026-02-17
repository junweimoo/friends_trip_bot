#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <pqxx/pqxx>
#include <string>
#include <memory>

class DatabaseManager {
public:
    DatabaseManager(const std::string& connection_string);
    void connect();
    void disconnect();
    pqxx::connection* getConnection();

private:
    std::string connection_string_;
    std::unique_ptr<pqxx::connection> connection_;
};

#endif // DATABASE_MANAGER_H

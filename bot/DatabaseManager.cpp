#include "DatabaseManager.h"
#include <iostream>

DatabaseManager::DatabaseManager(const std::string& connection_string)
    : connection_string_(connection_string) {}

void DatabaseManager::connect() {
    try {
        connection_ = std::make_unique<pqxx::connection>(connection_string_);
        if (connection_->is_open()) {
            std::cout << "Opened database successfully: " << connection_->dbname() << std::endl;
        } else {
            std::cerr << "Can't open database" << std::endl;
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

void DatabaseManager::disconnect() {
    if (connection_) {
        if (connection_ && connection_->is_open()) {
            connection_->close();
            std::cout << "Disconnected from database" << std::endl;
        }
    }
}

pqxx::connection* DatabaseManager::getConnection() {
    return connection_.get();
}

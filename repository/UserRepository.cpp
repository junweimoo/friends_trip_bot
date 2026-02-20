#include "UserRepository.h"
#include <iostream>
#include <pqxx/pqxx>

UserRepository::UserRepository(DatabaseManager& dbManager) : dbManager_(dbManager) {}

bool UserRepository::createUser(const User& user) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);
        txn.exec_params(
            "INSERT INTO users (user_id, chat_id, thread_id, name) VALUES ($1, $2, $3, $4)",
            user.user_id, user.chat_id, user.thread_id, user.name
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating user: " << e.what() << std::endl;
        return false;
    }
}

std::optional<User> UserRepository::getUser(long long userId, long long chatId, int threadId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return std::nullopt;

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_params(
            "SELECT user_id, chat_id, thread_id, name, gmt_created, gmt_modified FROM users WHERE user_id = $1 AND chat_id = $2 AND thread_id = $3",
            userId, chatId, threadId
        );

        if (res.empty()) return std::nullopt;

        const auto& row = res[0];
        return User{
            row["user_id"].as<long long>(),
            row["chat_id"].as<long long>(),
            row["thread_id"].as<int>(),
            row["name"].c_str(),
            row["gmt_created"].c_str(),
            row["gmt_modified"].c_str()
        };
    } catch (const std::exception& e) {
        std::cerr << "Error getting user: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool UserRepository::updateUser(const User& user) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_params(
            "UPDATE users SET name = $4 WHERE user_id = $1 AND chat_id = $2 AND thread_id = $3",
            user.user_id, user.chat_id, user.thread_id, user.name
        );
        txn.commit();
        return res.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error updating user: " << e.what() << std::endl;
        return false;
    }
}

bool UserRepository::deleteUser(long long userId, long long chatId, int threadId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_params(
            "DELETE FROM users WHERE user_id = $1 AND chat_id = $2 AND thread_id = $3",
            userId, chatId, threadId
        );
        txn.commit();
        return res.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting user: " << e.what() << std::endl;
        return false;
    }
}

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

bool UserRepository::registerUserWithDefaultTrip(const User& user) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);

        // 1. Insert user
        txn.exec_params(
            "INSERT INTO users (user_id, chat_id, thread_id, name) VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (user_id, chat_id, thread_id) DO NOTHING",
            user.user_id, user.chat_id, user.thread_id, user.name
        );

        // 2. Check if a trip already exists for this chat/thread
        pqxx::result tripRes = txn.exec_params(
            "SELECT trip_id FROM trips WHERE chat_id = $1 AND thread_id = $2 LIMIT 1",
            user.chat_id, user.thread_id
        );

        long long tripId;
        if (tripRes.empty()) {
            // 3. Create default trip if none exists
            pqxx::result newTripRes = txn.exec_params(
                "INSERT INTO trips (chat_id, thread_id, name) VALUES ($1, $2, 'default') RETURNING trip_id",
                user.chat_id, user.thread_id
            );
            tripId = newTripRes[0][0].as<long long>();
        } else {
            tripId = tripRes[0][0].as<long long>();
        }

        // 4. Create/Update chat with active trip
        txn.exec_params(
            "INSERT INTO chats (chat_id, thread_id, active_trip_id) VALUES ($1, $2, $3) "
            "ON CONFLICT (chat_id, thread_id) DO UPDATE SET active_trip_id = EXCLUDED.active_trip_id "
            "WHERE chats.active_trip_id IS NULL",
            user.chat_id, user.thread_id, tripId
        );

        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in registerUserWithDefaultTrip: " << e.what() << std::endl;
        return false;
    }
}

std::optional<User> UserRepository::getUser(long long userId, long long chatId, long long threadId) {
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
            row["thread_id"].as<long long>(),
            row["name"].c_str(),
            row["gmt_created"].c_str(),
            row["gmt_modified"].c_str()
        };
    } catch (const std::exception& e) {
        std::cerr << "Error getting user: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<User> UserRepository::getUsersByChatAndThread(long long chatId, long long threadId) {
    std::vector<User> users;
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return users;

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_params(
            "SELECT user_id, chat_id, thread_id, name, gmt_created, gmt_modified FROM users WHERE chat_id = $1 AND thread_id = $2",
            chatId, threadId
        );

        for (const auto& row : res) {
            users.emplace_back(User{
                row["user_id"].as<long long>(),
                row["chat_id"].as<long long>(),
                row["thread_id"].as<long long>(),
                row["name"].c_str(),
                row["gmt_created"].c_str(),
                row["gmt_modified"].c_str()
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting users by chat and thread: " << e.what() << std::endl;
    }
    return users;
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

bool UserRepository::deleteUser(long long userId, long long chatId, long long threadId) {
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

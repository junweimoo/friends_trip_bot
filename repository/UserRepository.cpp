#include "UserRepository.h"
#include <iostream>
#include <pqxx/pqxx>

UserRepository::UserRepository(DatabaseManager& dbManager) : dbManager_(dbManager) {}

bool UserRepository::createUser(const User& user) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error creating user: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);
        txn.exec(
            "INSERT INTO users (user_id, chat_id, thread_id, name) VALUES ($1, $2, $3, $4)",
            pqxx::params{user.user_id, user.chat_id, user.thread_id, user.name}
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
    if (!conn || !conn->is_open()) {
        std::cerr << "Error registering user with default trip: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);

        // 1. Insert user
        txn.exec(
            "INSERT INTO users (user_id, chat_id, thread_id, name) VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (user_id, chat_id, thread_id) DO NOTHING",
            pqxx::params{user.user_id, user.chat_id, user.thread_id, user.name}
        );

        // 2. Check if a trip already exists for this chat/thread
        pqxx::result tripRes = txn.exec(
            "SELECT trip_id FROM trips WHERE chat_id = $1 AND thread_id = $2 LIMIT 1",
            pqxx::params{user.chat_id, user.thread_id}
        );

        long long tripId;
        if (tripRes.empty()) {
            // 3. Create default trip if none exists
            pqxx::result newTripRes = txn.exec(
                "INSERT INTO trips (chat_id, thread_id, name) VALUES ($1, $2, 'default') RETURNING trip_id",
                pqxx::params{user.chat_id, user.thread_id}
            );
            tripId = newTripRes[0][0].as<long long>();

            // 4. Create/Update chat with active trip
            txn.exec(
                "INSERT INTO chats (chat_id, thread_id, active_trip_id) VALUES ($1, $2, $3) "
                "ON CONFLICT (chat_id, thread_id) DO UPDATE SET active_trip_id = EXCLUDED.active_trip_id "
                "WHERE chats.active_trip_id IS NULL",
                pqxx::params{user.chat_id, user.thread_id, tripId}
            );
        }

        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in registerUserWithDefaultTrip: " << e.what() << std::endl;
        return false;
    }
}

std::optional<User> UserRepository::getUser(long long userId, long long chatId, long long threadId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting user: database connection unavailable" << std::endl;
        return std::nullopt;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "SELECT user_id, chat_id, thread_id, name, gmt_created, gmt_modified FROM users WHERE user_id = $1 AND chat_id = $2 AND thread_id = $3",
            pqxx::params{userId, chatId, threadId}
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
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting users by chat and thread: database connection unavailable" << std::endl;
        return users;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "SELECT user_id, chat_id, thread_id, name, gmt_created, gmt_modified FROM users WHERE chat_id = $1 AND thread_id = $2",
            pqxx::params{chatId, threadId}
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
    if (!conn || !conn->is_open()) {
        std::cerr << "Error updating user: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "UPDATE users SET name = $4 WHERE user_id = $1 AND chat_id = $2 AND thread_id = $3",
            pqxx::params{user.user_id, user.chat_id, user.thread_id, user.name}
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
    if (!conn || !conn->is_open()) {
        std::cerr << "Error deleting user: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "DELETE FROM users WHERE user_id = $1 AND chat_id = $2 AND thread_id = $3",
            pqxx::params{userId, chatId, threadId}
        );
        txn.commit();
        return res.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting user: " << e.what() << std::endl;
        return false;
    }
}

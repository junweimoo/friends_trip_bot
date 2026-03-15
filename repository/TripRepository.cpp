#include "TripRepository.h"
#include "../database/DatabaseManager.h"
#include <iostream>
#include <pqxx/pqxx>

TripRepository::TripRepository(DatabaseManager& dbManager) : dbManager_(dbManager) {}

bool TripRepository::createDefaultChatAndTrip(long long chatId, long long threadId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error creating default chat and trip: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);

        pqxx::result res = txn.exec(
            "SELECT 1 FROM trips WHERE chat_id = $1 AND thread_id = $2 LIMIT 1",
            pqxx::params{chatId, threadId}
        );

        if (!res.empty()) {
            return false;
        }

        pqxx::result tripRes = txn.exec(
            "INSERT INTO trips (chat_id, thread_id, name) VALUES ($1, $2, 'default') RETURNING trip_id",
            pqxx::params{chatId, threadId}
        );

        if (tripRes.empty()) return false;
        long long newTripId = tripRes[0][0].as<long long>();

        txn.exec(
            "INSERT INTO chats (chat_id, thread_id, active_trip_id) VALUES ($1, $2, $3) "
            "ON CONFLICT (chat_id, thread_id) DO UPDATE SET active_trip_id = $3",
            pqxx::params{chatId, threadId, newTripId}
        );

        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in createDefaultChatAndTrip: " << e.what() << std::endl;
        return false;
    }
}

long long TripRepository::createTrip(long long chatId, long long threadId, const std::string& name) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error creating trip: database connection unavailable" << std::endl;
        return -1;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "INSERT INTO trips (chat_id, thread_id, name) VALUES ($1, $2, $3) RETURNING trip_id",
            pqxx::params{chatId, threadId, name}
        );
        txn.commit();
        
        if (res.empty()) return -1;
        return res[0][0].as<long long>();
    } catch (const std::exception& e) {
        std::cerr << "Error creating trip: " << e.what() << std::endl;
        return -1;
    }
}

std::optional<Trip> TripRepository::getTrip(long long tripId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting trip: database connection unavailable" << std::endl;
        return std::nullopt;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "SELECT trip_id, chat_id, thread_id, name, gmt_created FROM trips WHERE trip_id = $1",
            pqxx::params{tripId}
        );

        if (res.empty()) return std::nullopt;

        const auto& row = res[0];
        return Trip{
            row["trip_id"].as<long long>(),
            row["chat_id"].as<long long>(),
            row["thread_id"].as<long long>(),
            row["name"].c_str(),
            row["gmt_created"].c_str()
        };
    } catch (const std::exception& e) {
        std::cerr << "Error getting trip: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<Trip> TripRepository::getAllTrips(long long chatId, long long threadId) {
    std::vector<Trip> trips;
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting all trips: database connection unavailable" << std::endl;
        return trips;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "SELECT trip_id, chat_id, thread_id, name, gmt_created FROM trips WHERE chat_id = $1 AND thread_id = $2 ORDER BY trip_id ASC",
            pqxx::params{chatId, threadId}
        );

        for (const auto& row : res) {
            trips.emplace_back(Trip{
                row["trip_id"].as<long long>(),
                row["chat_id"].as<long long>(),
                row["thread_id"].as<long long>(),
                row["name"].c_str(),
                row["gmt_created"].c_str()
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting all trips: " << e.what() << std::endl;
    }
    return trips;
}

bool TripRepository::updateTrip(const Trip& trip) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error updating trip: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "UPDATE trips SET name = $2 WHERE trip_id = $1",
            pqxx::params{trip.trip_id, trip.name}
        );
        txn.commit();
        return res.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error updating trip: " << e.what() << std::endl;
        return false;
    }
}

bool TripRepository::deleteTrip(long long tripId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error deleting trip: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "DELETE FROM trips WHERE trip_id = $1",
            pqxx::params{tripId}
        );
        txn.commit();
        return res.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting trip: " << e.what() << std::endl;
        return false;
    }
}

bool TripRepository::updateActiveTrip(long long chatId, long long threadId, long long tripId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error updating active trip: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "UPDATE chats SET active_trip_id = $3 WHERE chat_id = $1 AND thread_id = $2",
            pqxx::params{chatId, threadId, tripId}
        );
        txn.commit();
        return res.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error updating active trip: " << e.what() << std::endl;
        return false;
    }
}

std::optional<Trip> TripRepository::getActiveTrip(long long chatId, long long threadId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting active trip: database connection unavailable" << std::endl;
        return std::nullopt;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "SELECT t.trip_id, t.chat_id, t.thread_id, t.name, t.gmt_created "
            "FROM trips t "
            "JOIN chats c ON t.trip_id = c.active_trip_id "
            "WHERE c.chat_id = $1 AND c.thread_id = $2",
            pqxx::params{chatId, threadId}
        );

        if (res.empty()) return std::nullopt;

        const auto& row = res[0];
        return Trip{
            row["trip_id"].as<long long>(),
            row["chat_id"].as<long long>(),
            row["thread_id"].as<long long>(),
            row["name"].c_str(),
            row["gmt_created"].c_str()
        };
    } catch (const std::exception& e) {
        std::cerr << "Error getting active trip: " << e.what() << std::endl;
        return std::nullopt;
    }
}

#include "DatabaseSchema.h"
#include <iostream>

namespace DatabaseSchema {

void createTables(DatabaseManager& dbManager) {
    pqxx::connection* conn = dbManager.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Database connection is not open. Cannot create tables." << std::endl;
        return;
    }

    try {
        pqxx::work txn(*conn);

        // Create users table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS users (
                user_id BIGINT,
                chat_id BIGINT,
                thread_id BIGINT,
                gmt_created TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                gmt_modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                name VARCHAR(255),
                PRIMARY KEY (user_id, chat_id, thread_id)
            );
        )");

        // Create trips table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS trips (
                trip_id BIGSERIAL PRIMARY KEY,
                chat_id BIGINT,
                thread_id BIGINT,
                name VARCHAR(255) NOT NULL,
                gmt_created TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            CREATE INDEX IF NOT EXISTS idx_trips_chat_id ON trips(chat_id);
        )");

        // Create chats table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS chats (
                chat_id BIGINT,
                thread_id BIGINT,
                active_trip_id BIGINT REFERENCES trips(trip_id) ON DELETE SET NULL,
                gmt_created TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                gmt_modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                PRIMARY KEY (chat_id, thread_id)
            );
        )");

        // Create payment_groups table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS payment_groups (
                group_id BIGSERIAL PRIMARY KEY,
                trip_id BIGINT NOT NULL REFERENCES trips(trip_id) ON DELETE CASCADE,
                name VARCHAR(255),
                gmt_created TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            CREATE INDEX IF NOT EXISTS idx_payment_groups_trip_id ON payment_groups(trip_id);
        )");

        // Create payment_records table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS payment_records (
                record_id BIGSERIAL PRIMARY KEY,
                group_id BIGINT NOT NULL REFERENCES payment_groups(group_id) ON DELETE CASCADE,
                trip_id BIGINT NOT NULL, 
                amount NUMERIC(15, 2),
                gmt_created TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            CREATE INDEX IF NOT EXISTS idx_payment_records_trip_id ON payment_records(trip_id);
        )");

        txn.commit();
        std::cout << "Tables created/verified successfully." << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error creating tables: " << e.what() << std::endl;
    }
}

}

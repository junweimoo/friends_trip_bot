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
                thread_id INTEGER,
                gmt_created TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                gmt_modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                name VARCHAR(255),
                PRIMARY KEY (user_id, chat_id, thread_id)
            );
        )");

        txn.commit();
        std::cout << "Tables created/verified successfully." << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error creating tables: " << e.what() << std::endl;
    }
}

}

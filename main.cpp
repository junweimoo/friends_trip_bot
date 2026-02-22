#include <iostream>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sstream>
#include <memory>
#include "bot/Bot.h"
#include "database/DatabaseManager.h"
#include "database/DatabaseSchema.h"
#include "handlers/Handlers.h"
#include "repository/UserRepository.h"
#include "repository/PaymentRepository.h"
#include "repository/TripRepository.h"
#include "service/UserService.h"

void loadEnv(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            setenv(key.c_str(), value.c_str(), 1);
        }
    }
}

int main() {
    // Load .env file
    loadEnv(".env");

    // Database connection
    const char* dbHost = std::getenv("POSTGRES_HOST");
    const char* dbPort = std::getenv("POSTGRES_PORT");
    const char* dbName = std::getenv("POSTGRES_DB");
    const char* dbUser = std::getenv("POSTGRES_USER");
    const char* dbPass = std::getenv("POSTGRES_PASSWORD");

    std::unique_ptr<DatabaseManager> db;

    std::unique_ptr<UserRepository> userRepo;
    std::unique_ptr<PaymentRepository> paymentRepo;
    std::unique_ptr<TripRepository> tripRepo;

    std::unique_ptr<UserService> userService;

    if (dbHost && dbPort && dbName && dbUser && dbPass) {
        std::stringstream ss;
        ss << "host=" << dbHost << " port=" << dbPort << " dbname=" << dbName
           << " user=" << dbUser << " password=" << dbPass;

        db = std::make_unique<DatabaseManager>(ss.str());
        db->connect();

        // Ensure tables exist
        DatabaseSchema::createTables(*db);

        userRepo = std::make_unique<UserRepository>(*db);
        paymentRepo = std::make_unique<PaymentRepository>(*db);
        tripRepo = std::make_unique<TripRepository>(*db);

        userService = std::make_unique<UserService>(*userRepo);
    } else {
        std::cerr << "Warning: Database environment variables not fully set. Skipping DB connection." << std::endl;
    }

    const char* tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
    if (!tokenEnv) {
        std::cerr << "Error: TELEGRAM_BOT_TOKEN environment variable not set." << std::endl;
        return 1;
    }

    std::string token(tokenEnv);
    bot::Bot myBot(token);

    // Register all handlers
    handlers::Services services{*userService};
    handlers::Repositories repos{*userRepo, *tripRepo, *paymentRepo};
    handlers::registerHandlers(myBot, services, repos);

    // Start the bot
    myBot.start();

    return 0;
}

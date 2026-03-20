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
#include "service/PaymentService.h"

void loadEnv(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t delimPos = line.find('=');
        if (delimPos != std::string::npos) {
            setenv(line.substr(0, delimPos).c_str(), line.substr(delimPos + 1).c_str(), 1);
        }
    }
}

std::string buildDbConnString() {
    const char* host = std::getenv("POSTGRES_HOST");
    const char* port = std::getenv("POSTGRES_PORT");
    const char* name = std::getenv("POSTGRES_DB");
    const char* user = std::getenv("POSTGRES_USER");
    const char* pass = std::getenv("POSTGRES_PASSWORD");

    if (!host || !port || !name || !user || !pass) return "";

    std::stringstream ss;
    ss << "host=" << host << " port=" << port << " dbname=" << name
       << " user=" << user << " password=" << pass;
    return ss.str();
}

int main() {
    loadEnv(".env");

    const char* tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
    if (!tokenEnv) {
        std::cerr << "Error: TELEGRAM_BOT_TOKEN environment variable not set." << std::endl;
        return 1;
    }

    std::string dbConnString = buildDbConnString();
    if (dbConnString.empty()) {
        std::cerr << "Error: Database environment variables not fully set." << std::endl;
        return 1;
    }

    // Database
    auto db = std::make_unique<DatabaseManager>(dbConnString);
    db->connect();
    DatabaseSchema::createTables(*db);

    // Repositories
    auto userRepo    = std::make_unique<UserRepository>(*db);
    auto paymentRepo = std::make_unique<PaymentRepository>(*db);
    auto tripRepo    = std::make_unique<TripRepository>(*db);

    // Bot
    bot::Bot myBot(tokenEnv);

    // Services
    auto userService    = std::make_unique<UserService>(*userRepo, myBot);
    auto paymentService = std::make_unique<PaymentService>(*paymentRepo, *tripRepo, myBot);

    // Register handlers and start
    handlers::Services services{*userService, *paymentService};
    handlers::Repositories repos{*userRepo, *tripRepo, *paymentRepo};
    handlers::registerHandlers(myBot, services, repos);

    myBot.start();
    return 0;
}
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sstream>
#include <memory>
#include "bot/Bot.h"
#include "database/DatabaseManager.h"
#include "handlers/Handlers.h"

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

    if (dbHost && dbPort && dbName && dbUser && dbPass) {
        std::stringstream ss;
        ss << "host=" << dbHost << " port=" << dbPort << " dbname=" << dbName
           << " user=" << dbUser << " password=" << dbPass;

        DatabaseManager db(ss.str());
        db.connect();
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
    handlers::registerHandlers(myBot);

    // Start the bot
    myBot.start();

    return 0;
}

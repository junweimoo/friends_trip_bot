#include <iostream>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sstream>
#include "bot/Bot.h"
#include "bot/DatabaseManager.h"

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

    // Register /start command handler
    myBot.registerCommandHandler("/start", [&myBot](const bot::Message& msg) {
        std::cout << "Received /start from " << msg.chat_id << std::endl;
        myBot.sendMessage(msg.chat_id, "Hello! I am your Friends Trip Bot.");
    });

    // Register generic text handler
    myBot.registerTextHandler([&myBot](const bot::Message& msg) {
        std::cout << "Received text from " << msg.chat_id << ": " << msg.text << std::endl;
        myBot.sendMessage(msg.chat_id, "You said: " + msg.text);
    });

    // Start the bot
    myBot.start();

    return 0;
}

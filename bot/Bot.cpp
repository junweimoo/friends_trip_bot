#include "Bot.h"
#include "TelegramTypes.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <curl/curl.h>

namespace bot {

// Helper for CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

Bot::Bot(const std::string& token)
    : token(token),
      baseUrl("https://api.telegram.org/bot" + token + "/"),
      running(false),
      lastUpdateId(0) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Bot::~Bot() {
    stop();
    curl_global_cleanup();
}

void Bot::start() {
    running = true;
    std::cout << "Bot started..." << std::endl;
    while (running) {
        try {
            poll();
        } catch (const std::exception& e) {
            std::cerr << "Error in bot loop: " << e.what() << std::endl;
            // Only sleep when an exception is caught to avoid tight-looping on persistent errors
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void Bot::stop() {
    running = false;
}

void Bot::registerCommandHandler(const std::string& command, CommandHandler handler) {
    commandHandlers[command] = handler;
}

void Bot::registerTextHandler(TextHandler handler) {
    textHandler = handler;
}

void Bot::sendMessage(long long chatId, const std::string& text) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char *output = curl_easy_escape(curl, text.c_str(), text.length());
        if(output) {
            std::string encodedText(output);
            std::string params = "chat_id=" + std::to_string(chatId) + "&text=" + encodedText;
            makeRequest("sendMessage", params);
            curl_free(output);
        }
        curl_easy_cleanup(curl);
    }
}

std::string Bot::makeRequest(const std::string& endpoint, const std::string& params) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        std::string url = baseUrl + endpoint;
        if (!params.empty()) {
            url += "?" + params;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 40L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

std::vector<Message> Bot::getUpdates() {
    std::string params = "offset=" + std::to_string(lastUpdateId) + "&timeout=30";
    std::string responseStr = makeRequest("getUpdates", params);

    std::vector<Message> messages;

    try {
        auto jsonResponse = json::parse(responseStr);
        GetUpdatesResponse resp = jsonResponse.get<GetUpdatesResponse>();

        if (resp.ok) {
            for (const auto& update : resp.result) {
                if (update.message.message_id != 0) {
                    Message msg;
                    msg.update_id = update.update_id;
                    msg.chat_id = update.message.chat.id;
                    msg.text = update.message.text;
                    msg.sender_name = update.message.from.first_name;
                    messages.push_back(msg);
                }
            }
        }
    } catch (const std::exception& e) {
        throw;
    }

    return messages;
}

void Bot::poll() {
    auto updates = getUpdates();
    for (const auto& msg : updates) {
        if (msg.update_id >= lastUpdateId) {
            lastUpdateId = msg.update_id + 1;
        }

        if (!msg.text.empty()) {
            if (msg.text[0] == '/') {
                size_t spacePos = msg.text.find(' ');
                std::string command = (spacePos == std::string::npos) ? msg.text : msg.text.substr(0, spacePos);

                if (commandHandlers.find(command) != commandHandlers.end()) {
                    commandHandlers[command](msg);
                }
            } else {
                if (textHandler) {
                    textHandler(msg);
                }
            }
        }
    }
}

}

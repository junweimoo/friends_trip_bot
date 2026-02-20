#include "Bot.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <future>

namespace bot {

using json = nlohmann::json;

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

void Bot::registerCallbackHandler(CallbackHandler handler) {
    callbackHandler = handler;
}

void Bot::registerConversation(std::unique_ptr<Conversation> conversation) {
    if (!conversation) return;
    long long chatId = conversation->getChatId();
    long long userId = conversation->getUserId();

    auto entry = std::make_shared<ConversationEntry>();
    entry->conversation = std::move(conversation);

    {
        // Use unique_lock here so that only one thread can write to conversations at a time
        std::unique_lock<std::shared_mutex> lock(conversationsMutex);
        conversations[{chatId, userId}] = entry;
    }
}

void Bot::sendMessage(long long chatId, const std::string& text, const InlineKeyboardMarkup* keyboard) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char *output = curl_easy_escape(curl, text.c_str(), text.length());
        if(output) {
            std::string encodedText(output);
            std::string url = baseUrl + "sendMessage?chat_id=" + std::to_string(chatId) + "&text=" + encodedText;

            if (keyboard) {
                json j = *keyboard;
                std::string jsonStr = j.dump();
                char *encodedKeyboard = curl_easy_escape(curl, jsonStr.c_str(), jsonStr.length());
                if (encodedKeyboard) {
                    url += "&reply_markup=" + std::string(encodedKeyboard);
                    curl_free(encodedKeyboard);
                }
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                 fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }

            curl_free(output);
        }
        curl_easy_cleanup(curl);
    }
}

void Bot::editMessage(long long chatId, long long messageId, const std::string& text, const InlineKeyboardMarkup* keyboard) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char *output = curl_easy_escape(curl, text.c_str(), text.length());
        if(output) {
            std::string encodedText(output);
            std::string url = baseUrl + "editMessageText?chat_id=" + std::to_string(chatId) +
                             "&message_id=" + std::to_string(messageId) +
                             "&text=" + encodedText;

            if (keyboard) {
                json j = *keyboard;
                std::string jsonStr = j.dump();
                char *encodedKeyboard = curl_easy_escape(curl, jsonStr.c_str(), jsonStr.length());
                if (encodedKeyboard) {
                    url += "&reply_markup=" + std::string(encodedKeyboard);
                    curl_free(encodedKeyboard);
                }
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                 fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }

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

std::vector<Update> Bot::getUpdates() {
    std::string params = "offset=" + std::to_string(lastUpdateId) + "&timeout=30";
    std::string responseStr = makeRequest("getUpdates", params);

    std::vector<Update> updates;

    try {
        auto jsonResponse = json::parse(responseStr);
        if (jsonResponse.contains("ok") && jsonResponse["ok"].get<bool>()) {
            if (jsonResponse.contains("result")) {
                for (const auto& item : jsonResponse["result"]) {
                    Update update;
                    item.get_to(update);
                    updates.push_back(update);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

    return updates;
}

void Bot::poll() {
    auto updates = getUpdates();
    for (const Update& update : updates) {
        if (update.update_id >= lastUpdateId) {
            lastUpdateId = update.update_id + 1;
        }

        // Handle Conversations
        long long chatId = 0;
        long long userId = 0;

        if (update.message.message_id != 0) {
            chatId = update.message.chat.id;
            userId = update.message.from.id;
        } else if (!update.callback_query.id.empty()) {
            chatId = update.callback_query.message.chat.id;
            userId = update.callback_query.from.id;
        }

        bool handled = false;
        if (chatId != 0 && userId != 0) {
            std::shared_ptr<ConversationEntry> entry = nullptr;
            {
                // Use shared_lock here since this is a read-only operation
                std::shared_lock<std::shared_mutex> lock(conversationsMutex);
                auto it = conversations.find({chatId, userId});
                if (it != conversations.end()) {
                    entry = it->second;
                }
            }

            if (entry) {
                std::thread([this, update, chatId, userId, entry]() {
                    bool closedNow = false;
                    {
                        // Use fine-grained lock specific to this conversation entry
                        std::lock_guard<std::mutex> lock(entry->mutex);
                        if (!entry->conversation->isClosed()) {
                            // Dispatch conversation
                            entry->conversation->handleUpdate(update);
                            closedNow = entry->conversation->isClosed();
                        } else {
                            // Already closed by another thread, do nothing
                            closedNow = true;
                        }
                    }

                    if (closedNow) {
                        // Use unique_lock here so that only one thread can erase from conversations at a time
                        std::unique_lock<std::shared_mutex> mapLock(conversationsMutex);
                        auto it = conversations.find({chatId, userId});
                        if (it != conversations.end() && it->second == entry) {
                            conversations.erase(it);
                        }
                    }
                }).detach();
                handled = true;
            }
        }

        if (handled) continue;

        // Handle Messages
        if (update.message.message_id != 0) {
            Message msg;
            msg.update_id = update.update_id;
            msg.message_id = update.message.message_id;
            msg.chat_id = update.message.chat.id;
            msg.sender_id = update.message.from.id;
            msg.sender_name = update.message.from.first_name;
            msg.text = update.message.text;

            if (!msg.text.empty()) {
                // Handle Commands
                if (msg.text[0] == '/') {
                    size_t spacePos = msg.text.find(' ');
                    std::string command = (spacePos == std::string::npos) ? msg.text : msg.text.substr(0, spacePos);
                    if (commandHandlers.find(command) != commandHandlers.end()) {
                        // Dispatch command
                        std::thread(commandHandlers[command], msg).detach();
                    }
                // Handle Text
                } else {
                    if (textHandler) {
                        // Dispatch text
                        std::thread(textHandler, msg).detach();
                    }
                }
            }
        }

        // Handle Callback Queries
        if (!update.callback_query.id.empty()) {
            CallbackQuery query;
            query.update_id = update.update_id;
            query.chat_id = update.callback_query.message.chat.id;
            query.message_id = update.callback_query.message.message_id;
            query.sender_id = update.callback_query.from.id;
            query.sender_name = update.callback_query.from.first_name;
            query.data = update.callback_query.data;
            if (callbackHandler) {
                // Dispatch callback
                std::thread(callbackHandler, query).detach();
            }
        }
    }
}

}

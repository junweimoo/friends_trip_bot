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

Bot::Bot(const std::string& token, Scheduler& scheduler)
    : token(token), scheduler(scheduler),
      baseUrl("https://api.telegram.org/bot" + token + "/"),
      running(false),
      lastUpdateId(0) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    scheduler.registerTask([this] { sweepExpiredCallbacks(); }, true, 0, 0, 0);
}

Bot::~Bot() {
    stop();
    curl_global_cleanup();
}

void Bot::setUsername(std::string u) {
    username = std::move(u);
}

const std::string& Bot::getBotUsername() const {
    return username;
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

std::string Bot::storeCallback(std::function<void()> callback, int expiryHours) {
    std::string key = std::to_string(callbackCounter_.fetch_add(1));
    StoredCallback entry{std::move(callback), std::chrono::steady_clock::now(), std::chrono::hours(expiryHours)};
    callbacks_.insert_or_assign(key, std::move(entry));
    return key;
}

std::optional<std::function<void()>> Bot::fetchCallback(const std::string& key) {
    std::optional<std::function<void()>> result;
    callbacks_.erase_if(key, [&result](auto& kv) {
        result = std::move(kv.second.callback);
        return true;
    });
    return result;
}

void Bot::sweepExpiredCallbacks() {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> expired;
    callbacks_.for_each([&](auto& kv) {
        if ((now - kv.second.storedAt) >= kv.second.expiry) {
            expired.push_back(kv.first);
        }
    });
    for (const auto& key : expired) {
        callbacks_.erase(key);
    }
}

void Bot::registerConversation(std::unique_ptr<Conversation> conversation) {
    if (!conversation) return;
    long long chatId = conversation->getChatId();
    long long userId = conversation->getUserId();

    auto entry = std::make_shared<ConversationEntry>();
    entry->conversation = std::move(conversation);

    conversations.insert_or_assign({chatId, userId}, entry);
}

long long Bot::sendMessage(long long chatId, const std::string& text, const InlineKeyboardMarkup* keyboard, const std::string& parseMode, const std::string& callbackType) {
    CURL *curl = curl_easy_init();
    long long messageId = -1;
    if(curl) {
        char *output = curl_easy_escape(curl, text.c_str(), text.length());
        if(output) {
            std::string encodedText(output);
            std::string url = baseUrl + "sendMessage?chat_id=" + std::to_string(chatId) + "&text=" + encodedText;

            if (!parseMode.empty()) {
                url += "&parse_mode=" + parseMode;
            }

            if (keyboard) {
                InlineKeyboardMarkup kb = *keyboard;
                if (!callbackType.empty()) {
                    for (auto& row : kb.inline_keyboard) {
                        for (auto& btn : row) {
                            if (btn.url.empty()) {
                                btn.callback_data = callbackType + "|" + btn.callback_data;
                            }
                        }
                    }
                }
                json j = kb;
                std::string jsonStr = j.dump();
                char *encodedKeyboard = curl_easy_escape(curl, jsonStr.c_str(), jsonStr.length());
                if (encodedKeyboard) {
                    url += "&reply_markup=" + std::string(encodedKeyboard);
                    curl_free(encodedKeyboard);
                }
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            std::string responseBuffer;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                 fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                try {
                    auto jsonResponse = json::parse(responseBuffer);
                    if (jsonResponse.contains("ok") && jsonResponse["ok"].get<bool>()) {
                        if (jsonResponse.contains("result") && jsonResponse["result"].contains("message_id")) {
                            messageId = jsonResponse["result"]["message_id"].get<long long>();
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "JSON parse error in sendMessage: " << e.what() << std::endl;
                }
            }

            curl_free(output);
        }
        curl_easy_cleanup(curl);
    }
    return messageId;
}

void Bot::editMessage(long long chatId, long long messageId, const std::string& text, const InlineKeyboardMarkup* keyboard, const std::string& parseMode) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char *output = curl_easy_escape(curl, text.c_str(), text.length());
        if(output) {
            std::string encodedText(output);
            std::string url = baseUrl + "editMessageText?chat_id=" + std::to_string(chatId) +
                             "&message_id=" + std::to_string(messageId) +
                             "&text=" + encodedText;

            if (!parseMode.empty()) {
                url += "&parse_mode=" + parseMode;
            }

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

            // Capture the response to prevent libcurl from printing it to stdout
            std::string responseBuffer;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                 fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }

            curl_free(output);
        }
        curl_easy_cleanup(curl);
    }
}

void Bot::answerCallbackQuery(const std::string& callbackQueryId, const std::string& text, bool showAlert) {
    CURL *curl = curl_easy_init();
    if(curl) {
        std::string url = baseUrl + "answerCallbackQuery?callback_query_id=" + callbackQueryId;
        if (!text.empty()) {
            char *encodedText = curl_easy_escape(curl, text.c_str(), text.length());
            if (encodedText) {
                url += "&text=" + std::string(encodedText);
                curl_free(encodedText);
            }
        }
        if (showAlert) {
            url += "&show_alert=true";
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        std::string responseBuffer;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
             fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
}

Chat Bot::getChat(long long chatId) {
    std::string response = makeRequest("getChat", "chat_id=" + std::to_string(chatId));
    Chat chat{};
    try {
        auto jsonResponse = json::parse(response);
        if (jsonResponse.contains("ok") && jsonResponse["ok"].get<bool>() && jsonResponse.contains("result")) {
            jsonResponse["result"].get_to(chat);
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error in getChat: " << e.what() << std::endl;
    }
    return chat;
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

        // Extract fields
        long long chatId = 0;
        long long userId = 0;
        Message msg;

        if (update.message.message_id != 0) {
            chatId = update.message.chat.id;
            userId = update.message.from.id;

            msg.update_id = update.update_id;
            msg.message_id = update.message.message_id;
            msg.chat_id = update.message.chat.id;
            msg.sender_id = update.message.from.id;
            msg.sender_name = update.message.from.first_name;
            msg.text = update.message.text;
        } else if (!update.callback_query.id.empty()) {
            chatId = update.callback_query.message.chat.id;
            userId = update.callback_query.from.id;
        }

        // Handle Commands
        bool isCommand = false;
        if (update.message.message_id != 0 && !msg.text.empty()) {
            if (msg.text[0] == '/') {
                size_t spacePos = msg.text.find(' ');
                std::string command = (spacePos == std::string::npos) ? msg.text : msg.text.substr(0, spacePos);
                size_t atPos = command.find('@');
                if (atPos != std::string::npos) command = command.substr(0, atPos);
                if (commandHandlers.find(command) != commandHandlers.end()) {
                    // Dispatch command
                    std::thread(commandHandlers[command], msg).detach();
                    isCommand = true;
                }
            }
        }

        // Handle Conversations
        bool isConversation = false;
        if (!isCommand && chatId != 0 && userId != 0) {
            std::shared_ptr<ConversationEntry> entry;
            auto key = std::make_pair(chatId, userId);
            conversations.if_contains(key, [&entry](const auto& kv) {
                entry = kv.second;
            });

            if (entry) {
                std::thread([this, update, key, entry]() {
                    bool closedNow = false;
                    {
                        // Use fine-grained lock specific to this conversation entry
                        std::lock_guard<std::mutex> lock(entry->mutex);
                        if (!entry->conversation->isClosed()) {
                            entry->conversation->handleUpdate(update);
                            closedNow = entry->conversation->isClosed();
                        } else {
                            closedNow = true;
                        }
                    }

                    if (closedNow) {
                        conversations.erase_if(key, [&entry](auto& kv) {
                            return kv.second == entry;
                        });
                    }
                }).detach();
                isConversation = true;
            }
        }

        // Handle Text Messages
        if (!isCommand && !isConversation && update.message.message_id != 0 && !msg.text.empty()) {
            if (textHandler) {
                // Dispatch text
                std::thread(textHandler, msg).detach();
            }
        }

        // Handle Callback Queries — de-encapsulate type and route to typed handler
        if (!isCommand && !isConversation && !update.callback_query.id.empty()) {
            const std::string& rawData = update.callback_query.data;
            size_t sep = rawData.find('|');
            if (sep != std::string::npos) {
                std::string type = rawData.substr(0, sep);
                auto it = callbackHandlers.find(type);
                if (it != callbackHandlers.end()) {
                    CallbackQuery query;
                    query.id = update.callback_query.id;
                    query.update_id = update.update_id;
                    query.chat_id = update.callback_query.message.chat.id;
                    query.message_id = update.callback_query.message.message_id;
                    query.sender_id = update.callback_query.from.id;
                    query.sender_name = update.callback_query.from.first_name;
                    query.data = rawData.substr(sep + 1);
                    query.message_text = update.callback_query.message.text;
                    std::thread(it->second, query).detach();
                }
            }
        }
    }
}

}

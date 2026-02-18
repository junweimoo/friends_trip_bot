#ifndef BOT_H
#define BOT_H

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <atomic>
#include "TelegramTypes.h"

namespace bot {

struct Message {
    long long chat_id;
    std::string text;
    std::string sender_name;
    long long update_id;
};

using CommandHandler = std::function<void(const Message&)>;
using TextHandler = std::function<void(const Message&)>;
using CallbackHandler = std::function<void(const CallbackQuery&)>;

class Bot {
public:
    explicit Bot(const std::string& token);
    ~Bot();

    void start();
    void stop();

    void registerCommandHandler(const std::string& command, CommandHandler handler);
    void registerTextHandler(TextHandler handler);
    void registerCallbackHandler(CallbackHandler handler);

    void sendMessage(long long chatId, const std::string& text, const InlineKeyboardMarkup* keyboard = nullptr);

private:
    std::string token;
    std::string baseUrl;
    std::atomic<bool> running;
    long long lastUpdateId;

    std::map<std::string, CommandHandler> commandHandlers;
    TextHandler textHandler;
    CallbackHandler callbackHandler;

    void poll();
    std::vector<Update> getUpdates();
    std::string makeRequest(const std::string& endpoint, const std::string& params = "");
};

}

#endif // BOT_H

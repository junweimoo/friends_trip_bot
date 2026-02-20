#ifndef BOT_H
#define BOT_H

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "Conversation.h"
#include "InternalTypes.h"
#include "TelegramTypes.h"

namespace bot {

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
    void registerConversation(std::unique_ptr<Conversation> conversation);

    void sendMessage(long long chatId, const std::string& text, const InlineKeyboardMarkup* keyboard = nullptr);
    void editMessage(long long chatId, long long messageId, const std::string& text, const InlineKeyboardMarkup* keyboard = nullptr);

private:
    std::string token;
    std::string baseUrl;
    std::atomic<bool> running;
    long long lastUpdateId;

    std::map<std::string, CommandHandler> commandHandlers;
    TextHandler textHandler;
    CallbackHandler callbackHandler;

    struct ConversationEntry {
        std::mutex mutex;
        std::unique_ptr<Conversation> conversation;
    };

    // Key: {chat_id, user_id}
    std::map<std::pair<long long, long long>, std::shared_ptr<ConversationEntry>> conversations;
    std::shared_mutex conversationsMutex;

    void poll();
    std::vector<Update> getUpdates();
    std::string makeRequest(const std::string& endpoint, const std::string& params = "");
};

}

#endif // BOT_H

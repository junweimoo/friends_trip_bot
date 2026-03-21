#ifndef BOT_H
#define BOT_H

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#include <parallel_hashmap/phmap.h>

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

    void setUsername(std::string username);
    const std::string& getBotUsername() const;

    template<typename F>
    void registerCommandHandler(std::string command, F&& handler) {
        commandHandlers.insert_or_assign(std::move(command), std::forward<F>(handler));
    }

    template<typename F>
    void registerTextHandler(F&& handler) {
        textHandler = std::forward<F>(handler);
    }

    template<typename F>
    void registerCallbackHandler(F&& handler) {
        callbackHandler = std::forward<F>(handler);
    }

    void registerConversation(std::unique_ptr<Conversation> conversation);

    long long sendMessage(long long chatId, const std::string& text, const InlineKeyboardMarkup* keyboard = nullptr, const std::string& parseMode = "");
    void editMessage(long long chatId, long long messageId, const std::string& text, const InlineKeyboardMarkup* keyboard = nullptr, const std::string& parseMode = "");
    void answerCallbackQuery(const std::string& callbackQueryId, const std::string& text = "", bool showAlert = false);
    Chat getChat(long long chatId);

private:
    std::string token;
    std::string username;
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

    struct PairHash {
        size_t operator()(const std::pair<long long, long long>& p) const {
            size_t h1 = phmap::Hash<long long>{}(p.first);
            size_t h2 = phmap::Hash<long long>{}(p.second);
            return phmap::HashState::combine(0, h1, h2);
        }
    };

    // Key: {chat_id, user_id}
    phmap::parallel_flat_hash_map<
        std::pair<long long, long long>,
        std::shared_ptr<ConversationEntry>,
        PairHash
    > conversations;

    void poll();
    std::vector<Update> getUpdates();
    std::string makeRequest(const std::string& endpoint, const std::string& params = "");
};

}

#endif // BOT_H
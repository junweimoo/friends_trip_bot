#ifndef FRIENDS_TRIP_BOT_CONVERSATION_H
#define FRIENDS_TRIP_BOT_CONVERSATION_H

#include <functional>
#include <map>
#include "InternalTypes.h"
#include "TelegramTypes.h"

namespace bot {

using ConversationHandler = std::function<void(const bot::Update&)>;

class Bot;

class Conversation {
public:
    explicit Conversation(long long chat_id, long long user_id, Bot& bot);
    virtual ~Conversation() = default;

    virtual void handleUpdate(const bot::Update& update) = 0;
    virtual bool isClosed() const = 0;

    long long getChatId() const;
    long long getUserId() const;

protected:
    long long chat_id;
    long long user_id;
    Bot& bot_;
};

} // namespace bot

#endif //FRIENDS_TRIP_BOT_CONVERSATION_H

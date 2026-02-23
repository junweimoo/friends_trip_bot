#include "Conversation.h"

namespace bot {

Conversation::Conversation(long long chat_id, long long thread_id, long long user_id, Bot& bot_)
    : chat_id(chat_id), thread_id(thread_id), user_id(user_id), bot_(bot_) {}

long long Conversation::getChatId() const {
    return chat_id;
}

long long Conversation::getThreadId() const {
    return thread_id;
}

long long Conversation::getUserId() const {
    return user_id;
}

} // namespace bot

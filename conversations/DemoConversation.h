#ifndef DEMO_CONVERSATION_H
#define DEMO_CONVERSATION_H

#include "../bot/Conversation.h"
#include <string>
#include <unordered_map>

namespace bot {

class DemoConversation : public Conversation {
public:
    DemoConversation(long long chat_id, long long user_id, Bot& bot);
    ~DemoConversation() override;

    void handleUpdate(const bot::Update& update) override;
    bool isClosed() const override;

private:
    void handleStep1(const bot::Update& update);
    void handleStep2(const bot::Update& update);

    int step_;
    bool closed_;
    std::string string1_;
    std::string string2_;
    std::unordered_map<int, ConversationHandler> conversationHandlers;
};

} // namespace bot

#endif // DEMO_CONVERSATION_H

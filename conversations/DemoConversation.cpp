#include "DemoConversation.h"

#include <iostream>

#include "../bot/Bot.h"

namespace bot {

DemoConversation::DemoConversation(long long chat_id, long long user_id, Bot& bot)
    : Conversation(chat_id, user_id, bot), step_(1), closed_(false) {
    conversationHandlers[1] = [this](const bot::Update& u) { handleStep1(u); };
    conversationHandlers[2] = [this](const bot::Update& u) { handleStep2(u); };
}

DemoConversation::~DemoConversation() {
    std::cout << "Destroying DemoConversation" << std::endl;
}

void DemoConversation::handleUpdate(const bot::Update& update) {
    if (closed_) return;

    if (conversationHandlers.find(step_) != conversationHandlers.end()) {
        conversationHandlers[step_](update);
    }
}

void DemoConversation::handleStep1(const bot::Update& update) {
    if (update.message.message_id == 0 || update.message.text.empty()) {
        return;
    }
    string1_ = update.message.text;
    step_ = 2;
    bot_.sendMessage(chat_id, "Got it. Now please enter string 2:");
}

void DemoConversation::handleStep2(const bot::Update& update) {
    if (update.message.message_id == 0 || update.message.text.empty()) {
        return;
    }
    string2_ = update.message.text;
    step_ = 3;

    std::string result = "Result: " + string1_ + " " + string2_;
    bot_.sendMessage(chat_id, result);

    closed_ = true;
}

bool DemoConversation::isClosed() const {
    return closed_;
}

} // namespace bot

#ifndef FRIENDS_TRIP_BOT_SIMPLIFYPAYMENTSCONVERSATION_H
#define FRIENDS_TRIP_BOT_SIMPLIFYPAYMENTSCONVERSATION_H

#include "../algorithm/DebtSimplifier.h"
#include "../bot/Conversation.h"
#include "../repository/PaymentRepository.h"
#include "../repository/TripRepository.h"
#include "../repository/UserRepository.h"
#include <unordered_map>
#include <vector>
#include <string>

class SimplifyPaymentsConversation : public bot::Conversation {
public:
    SimplifyPaymentsConversation(long long chat_id, long long thread_id, long long user_id,
        bot::Bot& bot, UserRepository& userRepo, TripRepository& tripRepo, PaymentRepository& payRepo);
    ~SimplifyPaymentsConversation() override;

    void handleUpdate(const bot::Update& update) override;
    bool isClosed() const override;

private:
    enum class State {
        SelectingCurrency,
        CollectingExchangeRate,
    };

    void sendCurrencySelection(bool edit);
    void handleCurrencySelection(const bot::Update& update);
    void sendExchangeRatePrompt(bool edit);
    void handleExchangeRateInput(const bot::Update& update);
    void computeAndDisplayResults();
    void closeConversation();

    State currentState_;
    bool closed_;
    long long active_message_id_;

    Trip trip_;
    std::vector<PaymentGroup> paymentGroups_;
    std::unordered_map<long long, User> users_;

    UserRepository& userRepo_;
    TripRepository& tripRepo_;
    PaymentRepository& payRepo_;

    std::string targetCurrency_;
    std::vector<std::string> foreignCurrencies_;
    size_t currentForeignCurrencyIndex_;
    std::unordered_map<std::string, double> exchangeRates_;
};

#endif //FRIENDS_TRIP_BOT_SIMPLIFYPAYMENTSCONVERSATION_H
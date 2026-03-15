#ifndef FRIENDS_TRIP_BOT_RECORDPAYMENTCONVERSATION_H
#define FRIENDS_TRIP_BOT_RECORDPAYMENTCONVERSATION_H

#include "../bot/Conversation.h"
#include "../repository/PaymentRepository.h"
#include "../repository/TripRepository.h"
#include "../repository/UserRepository.h"
#include <unordered_map>
#include <string>

class RecordPaymentConversation : public bot::Conversation {
public:
    RecordPaymentConversation(long long chat_id, long long thread_id, long long user_id, bot::Bot& bot, UserRepository& userRepo, TripRepository& tripRepo, PaymentRepository& payRepo);
    ~RecordPaymentConversation() override;

    void handleUpdate(const bot::Update& update) override;
    bool isClosed() const override;

private:
    enum class State {
        Description,
        Amount,
        Currency,
        Payer,
        Recipient,
        ManualRecipient,
        ManualAmount,
        EqualSplit,
        SingleRecipient,
    };

    void handleDescription(const bot::Update& update);
    void handleAmount(const bot::Update& update);
    void handleCurrency(const bot::Update& update);
    void handlePayer(const bot::Update& update);
    void handleRecipient(const bot::Update& update);
    void handleSingleRecipient(const bot::Update& update);
    void handleEqualSplitRecipients(const bot::Update& update);
    void handleManualRecipient(const bot::Update& update);
    void handleManualAmount(const bot::Update& update);

    void sendSingleRecipients(bool editMessage);
    void sendEqualSplitRecipients(bool editMessage);
    void sendManualRecipients(bool editMessage);
    void completeConversation();
    void cancelConversation();
    void distributeEqually();

    std::string createCallbackData(State targetState, const std::string& data);
    bool parseCallbackData(const std::string& jsonStr, State& targetState, std::string& data);

    std::mutex mutex_;

    State currentState_;
    bool closed;
    long long active_message_id;

    std::unordered_map<long long, User> users;
    Trip trip;

    // Allocated amounts stored as minor units (same currency as paymentGroup.total_amount)
    std::unordered_map<long long, long long> allocatedAmounts;
    double pendingRawAmount_ = 0.0;
    long long current_recipient_id;

    PaymentGroup paymentGroup;

    UserRepository& userRepo_;
    TripRepository& tripRepo_;
    PaymentRepository& payRepo_;

};

#endif //FRIENDS_TRIP_BOT_RECORDPAYMENTCONVERSATION_H
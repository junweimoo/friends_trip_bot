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

    std::string createCallbackData(int targetStep, const std::string& data);
    bool parseCallbackData(const std::string& jsonStr, int& targetStep, std::string& data);

    std::mutex mutex_;

    int step;
    bool closed;
    long long active_message_id;

    std::unordered_map<long long, User> users;
    Trip trip;

    long long chat_id;
    long long thread_id;
    long long user_id;

    std::unordered_map<long long, double> allocatedAmounts;
    long long current_recipient_id;

    PaymentGroup paymentGroup;

    UserRepository& userRepo_;
    TripRepository& tripRepo_;
    PaymentRepository& payRepo_;

};

#endif //FRIENDS_TRIP_BOT_RECORDPAYMENTCONVERSATION_H
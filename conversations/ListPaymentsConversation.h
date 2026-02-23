#ifndef FRIENDS_TRIP_BOT_LISTPAYMENTSCONVERSATION_H
#define FRIENDS_TRIP_BOT_LISTPAYMENTSCONVERSATION_H

#include "../bot/Conversation.h"
#include "../repository/PaymentRepository.h"
#include "../repository/TripRepository.h"
#include "../repository/UserRepository.h"
#include <unordered_map>
#include <vector>

class ListPaymentsConversation : public bot::Conversation {
public:
    ListPaymentsConversation(long long chat_id, long long thread_id, long long user_id, bot::Bot& bot, UserRepository& userRepo, TripRepository& tripRepo, PaymentRepository& payRepo);
    ~ListPaymentsConversation() override;

    void handleUpdate(const bot::Update& update) override;
    bool isClosed() const override;

private:
    void computeNetBalances();
    void sendCurrentPage(bool editMessage = false);
    void handlePageChange(const bot::Update& update);
    void closeConversation();

    int pageSize;
    int currentPage;
    int totalPages;
    bool closed;
    long long active_message_id;

    Trip trip;
    std::vector<PaymentGroup> paymentGroups;
    std::unordered_map<long long, User> users;
    std::unordered_map<long long, std::unordered_map<std::string, double>> netBalances;

    UserRepository& userRepo_;
    TripRepository& tripRepo_;
    PaymentRepository& payRepo_;
};

#endif //FRIENDS_TRIP_BOT_LISTPAYMENTSCONVERSATION_H
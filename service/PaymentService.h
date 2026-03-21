#ifndef FRIENDS_TRIP_BOT_PAYMENTSERVICE_H
#define FRIENDS_TRIP_BOT_PAYMENTSERVICE_H

#include <optional>
#include "../repository/PaymentRepository.h"
#include "../repository/TripRepository.h"
#include "../repository/UserRepository.h"
#include "../bot/Bot.h"

class PaymentService {
public:
    explicit PaymentService(PaymentRepository& paymentRepository, TripRepository& tripRepository,
                            UserRepository& userRepository, bot::Bot& bot);

    std::optional<PaymentGroup> undoLastPaymentInActiveTrip(long long chatId, long long threadId);
    void logSimplifiedPayment(const PaymentGroup& paymentGroup, const std::string& fromName,
                              const std::string& toName, long long groupChatId);

private:
    PaymentRepository& paymentRepository_;
    TripRepository& tripRepository_;
    UserRepository& userRepository_;
    bot::Bot& bot_;
};


#endif //FRIENDS_TRIP_BOT_PAYMENTSERVICE_H
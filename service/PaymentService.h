#ifndef FRIENDS_TRIP_BOT_PAYMENTSERVICE_H
#define FRIENDS_TRIP_BOT_PAYMENTSERVICE_H

#include <optional>
#include "../repository/PaymentRepository.h"
#include "../repository/TripRepository.h"
#include "../bot/Bot.h"

class PaymentService {
public:
    explicit PaymentService(PaymentRepository& paymentRepository, TripRepository& tripRepository, bot::Bot& bot);

    std::optional<PaymentGroup> undoLastPaymentInActiveTrip(long long chatId, long long threadId);

private:
    PaymentRepository& paymentRepository_;
    TripRepository& tripRepository_;
    bot::Bot& bot_;
};


#endif //FRIENDS_TRIP_BOT_PAYMENTSERVICE_H
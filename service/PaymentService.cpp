//
// Created by Moo Jun Wei on 20/3/26.
//

#include "PaymentService.h"
#include <iostream>

PaymentService::PaymentService(PaymentRepository& paymentRepository, TripRepository& tripRepository, bot::Bot& bot)
    : paymentRepository_(paymentRepository), tripRepository_(tripRepository), bot_(bot) {}

std::optional<PaymentGroup> PaymentService::undoLastPaymentInActiveTrip(long long chatId, long long threadId) {
    auto activeTrip = tripRepository_.getActiveTrip(chatId, threadId);
    if (!activeTrip.has_value()) {
        std::cerr << "No active trip found for chatId=" << chatId << " threadId=" << threadId << std::endl;
        return std::nullopt;
    }
    auto deleted = paymentRepository_.deleteLastPaymentGroup(activeTrip->trip_id);
    if (deleted.has_value()) {
        bot_.sendMessage(chatId, "Deleted payment: " + deleted->name);
    } else {
        bot_.sendMessage(chatId, "There are no payments in this group yet.");
    }
    return deleted;
}
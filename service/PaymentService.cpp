//
// Created by Moo Jun Wei on 20/3/26.
//

#include "PaymentService.h"
#include <iostream>
#include <sstream>

PaymentService::PaymentService(PaymentRepository& paymentRepository, TripRepository& tripRepository,
                               UserRepository& userRepository, bot::Bot& bot)
    : paymentRepository_(paymentRepository), tripRepository_(tripRepository),
      userRepository_(userRepository), bot_(bot) {}

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

void PaymentService::logSimplifiedPayment(const PaymentGroup& paymentGroup, const std::string& fromName,
                                           const std::string& toName, long long groupChatId) {
    if (!paymentRepository_.createPaymentGroup(paymentGroup)) {
        std::cerr << "Failed to log simplified payment for trip " << paymentGroup.trip_id << std::endl;
        return;
    }

    std::stringstream groupMsg;
    groupMsg << "\xf0\x9f\x92\xb8 <b>" << fromName << "</b> paid <b>" << toName
             << "</b>: <b>" << paymentGroup.total_amount.toHumanReadable() << "</b>";
    bot_.sendMessage(groupChatId, groupMsg.str(), nullptr, "HTML");
}
#include "Handlers.h"
#include "../conversations/RecordPaymentConversation.h"
#include <iostream>
#include <memory>

#include "../conversations/ListPaymentsConversation.h"
#include "../conversations/SimplifyPaymentsConversation.h"
#include "../conversations/TripsConversation.h"

namespace handlers {

void registerHandlers(bot::Bot& bot, const Services& services, const Repositories& repos) {
    // register handler
    bot.registerCommandHandler("/register", [&userService = services.userService](const bot::Message& msg) {
        User user;
        user.user_id = msg.sender_id;
        user.chat_id = msg.chat_id;
        user.thread_id = 0;
        user.name = msg.sender_name;

        userService.registerUser(user);
    });

    // record payment handler
    bot.registerCommandHandler("/pay", [&bot, &repos](const bot::Message& msg) {
        auto convo = std::make_unique<RecordPaymentConversation>(
            msg.chat_id, 0, msg.sender_id,
            bot, repos.userRepository, repos.tripRepository, repos.paymentRepository);
        bot.registerConversation(std::move(convo));
    });

    // list payments handler
    bot.registerCommandHandler("/list", [&bot, &repos](const bot::Message& msg) {
        auto convo = std::make_unique<ListPaymentsConversation>(
            msg.chat_id, 0, msg.sender_id,
            bot, repos.userRepository, repos.tripRepository, repos.paymentRepository);
        bot.registerConversation(std::move(convo));
    });

    // simplify payments handler
    bot.registerCommandHandler("/simplify", [&bot, &repos](const bot::Message& msg) {
        auto convo = std::make_unique<SimplifyPaymentsConversation>(
            msg.chat_id, 0, msg.sender_id,
            bot, repos.userRepository, repos.tripRepository, repos.paymentRepository);
        bot.registerConversation(std::move(convo));
    });

    // trips (create, list, delete) handler
    bot.registerCommandHandler("/trips", [&bot, &repos](const bot::Message& msg) {
        auto convo = std::make_unique<TripsConversation>(
            msg.chat_id, msg.sender_id, bot, repos.tripRepository, repos.userRepository);
        bot.registerConversation(std::move(convo));
    });

    // undo last payment handler
    bot.registerCommandHandler("/undo", [&paymentService = services.paymentService](const bot::Message& msg) {
        paymentService.undoLastPaymentInActiveTrip(msg.chat_id, 0);
    });
}

} // namespace handlers
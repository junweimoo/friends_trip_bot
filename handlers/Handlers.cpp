#include "Handlers.h"
#include "../conversations/RecordPaymentConversation.h"
#include <iostream>
#include <memory>

#include "../conversations/ListPaymentsConversation.h"

namespace handlers {

void registerHandlers(bot::Bot& bot, const Services& services, const Repositories& repos) {
    // register handler
    bot.registerCommandHandler("/register", [&bot, &userService = services.userService](const bot::Message& msg) {
        User user;
        user.user_id = msg.sender_id;
        user.chat_id = msg.chat_id;
        user.thread_id = 0;
        user.name = msg.sender_name;

        if (userService.registerUser(user)) {
            bot.sendMessage(msg.chat_id, "Successfully registered!");
        } else {
            bot.sendMessage(msg.chat_id, "An error occured during registration.");
        }
    });

    // record payment handler
    bot.registerCommandHandler("/pay", [&bot, &repos](const bot::Message& msg) {
        auto convo = std::make_unique<RecordPaymentConversation>(
            msg.chat_id, 0, msg.sender_id,
            bot, repos.userRepository, repos.tripRepository, repos.paymentRepository);
        bot.registerConversation(std::move(convo));
    });

    // list payments handler
    bot.registerCommandHandler("/listpayments", [&bot, &repos](const bot::Message& msg) {
        auto convo = std::make_unique<ListPaymentsConversation>(
            msg.chat_id, 0, msg.sender_id,
            bot, repos.userRepository, repos.tripRepository, repos.paymentRepository);
        bot.registerConversation(std::move(convo));
    });
}

} // namespace handlers

#include "Handlers.h"
#include "../conversations/RecordPaymentConversation.h"
#include <iostream>
#include <memory>

#include "../conversations/ListPaymentsConversation.h"
#include "../conversations/SimplifyPaymentsConversation.h"
#include "../conversations/TripsConversation.h"

namespace handlers {

void registerHandlers(bot::Bot& bot, const Services& services, const Repositories& repos) {
    // start handler
    bot.registerCommandHandler("/start", [&bot, &userService = services.userService](const bot::Message& msg) {
        // Check for deep link parameter: "/start register<chat_id>"
        std::string param;
        size_t spacePos = msg.text.find(' ');
        if (spacePos != std::string::npos) {
            param = msg.text.substr(spacePos + 1);
        }

        if (param.rfind("register", 0) == 0) {
            std::string chatIdStr = param.substr(8); // length of "register"
            try {
                long long groupChatId = std::stoll(chatIdStr);
                User user;
                user.user_id = msg.sender_id;
                user.chat_id = groupChatId;
                user.thread_id = 0;
                user.name = msg.sender_name;
                userService.completeRegistration(user);
            } catch (...) {
                bot.sendMessage(msg.chat_id, "Invalid registration link.");
            }
            return;
        }

        const std::string helpText =
            "<b>Welcome to Friends Trip Bot!</b>\n"
            "I can help you to track shared expenses in a group chat.\n\n"
            "<b>To get started:</b> Invite me to a group chat, then run /register in the chat.\n\n"
            "<b>Commands:</b>\n"
            "/register - Register yourself in this chat\n"
            "/trips - Manage trips (create, list, delete)\n"
            "/pay - Record a payment\n"
            "/list - List all payments\n"
            "/simplify - Simplify debts\n"
            "/undo - Undo the last payment";
        bot.sendMessage(msg.chat_id, helpText, nullptr, "HTML");
    });

    // register handler
    bot.registerCommandHandler("/register", [&bot, &userService = services.userService](const bot::Message& msg) {
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
    bot.registerCommandHandler("/simplify", [&bot, &repos, &paymentService = services.paymentService](const bot::Message& msg) {
        auto convo = std::make_unique<SimplifyPaymentsConversation>(
            msg.chat_id, 0, msg.sender_id,
            bot, repos.userRepository, repos.tripRepository, repos.paymentRepository, paymentService);
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

    // Log payment callback handler (from simplify DMs)
    bot.registerCallbackHandler("lp", [&bot](const bot::CallbackQuery& query) {
        auto callback = bot.fetchCallback(query.data);
        if (callback) {
            (*callback)();
            bot.answerCallbackQuery(query.id, "Payment logged!");
            bot.editMessage(query.chat_id, query.message_id,
                query.message_text + "\n\n\xe2\x9c\x85 Paid", nullptr, "");
        } else {
            bot.answerCallbackQuery(query.id, "This button has expired.", true);
        }
    });
}

} // namespace handlers
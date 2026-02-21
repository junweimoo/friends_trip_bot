#include "Handlers.h"
#include "../conversations/DemoConversation.h"
#include <iostream>
#include <memory>

namespace handlers {

void registerHandlers(bot::Bot& bot, const Services& services) {
    // Register /start command handler
    bot.registerCommandHandler("/start", [&bot](const bot::Message& msg) {
        std::cout << "Received /start from " << msg.chat_id << std::endl;
        bot.sendMessage(msg.chat_id, "Hello! I am your Friends Trip Bot.");
    });

    // Register /menu command to demonstrate Inline Keyboard
    bot.registerCommandHandler("/menu", [&bot](const bot::Message& msg) {
        bot::InlineKeyboardMarkup keyboard;
        bot::InlineKeyboardButton btn1{"Option 1", "opt_1"};
        bot::InlineKeyboardButton btn2{"Option 2", "opt_2"};

        // Add buttons in a single row
        keyboard.inline_keyboard = {{btn1, btn2}};

        bot.sendMessage(msg.chat_id, "Please choose an option:", &keyboard);
    });

    // Register /convo command to start DemoConversation
    bot.registerCommandHandler("/convo", [&bot](const bot::Message& msg) {
        auto convo = std::make_unique<bot::DemoConversation>(msg.chat_id, msg.sender_id, bot);
        bot.registerConversation(std::move(convo));

        bot.sendMessage(msg.chat_id, "Please enter string 1:");
    });

    // Register callback handler to edit the message when a button is pressed
    bot.registerCallbackHandler([&bot](const bot::CallbackQuery& query) {
        std::cout << "Received callback: " << query.data << " from " << query.sender_name << std::endl;

        bot::InlineKeyboardMarkup keyboard;
        bot::InlineKeyboardButton btn1{"Option 1", "opt_1"};
        bot::InlineKeyboardButton btn2{"Option 2", "opt_2"};
        keyboard.inline_keyboard = {{btn1, btn2}};

        std::string newText = query.data + " pressed";
        bot.editMessage(query.chat_id, query.message_id, newText, &keyboard);
    });

    // Register generic text handler
    bot.registerTextHandler([&bot](const bot::Message& msg) {
        std::cout << "Received text from " << msg.chat_id << ": " << msg.text << std::endl;
        bot.sendMessage(msg.chat_id, "You said: " + msg.text);
    });

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

    // pay handler

}

} // namespace handlers

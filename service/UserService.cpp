#include "UserService.h"
#include <iostream>

UserService::UserService(UserRepository& userRepository, bot::Bot& bot)
    : userRepository_(userRepository), bot_(bot) {}

bool UserService::registerUser(const User& user) {
    const std::string& botUsername = bot_.getBotUsername();
    bot::InlineKeyboardMarkup keyboard;
    bot::InlineKeyboardButton button;
    button.text = "Register";
    button.url = "https://t.me/" + botUsername + "?start=register" + std::to_string(user.chat_id);
    keyboard.inline_keyboard.push_back({button});
    bot_.sendMessage(user.chat_id,
        "Tap on the Register button to register yourself in this chat.\n"
        "Check registered users with /trips.",
        &keyboard);
    return true;
}

bool UserService::completeRegistration(const User& user) {
    bool success = userRepository_.registerUserWithDefaultTrip(user);
    if (success) {
        bot::Chat chat = bot_.getChat(user.chat_id);
        std::string chatName = chat.title.empty() ? "the chat" : chat.title;
        bot_.sendMessage(user.user_id, "Successfully registered in " + chatName + "!");
        bot_.sendMessage(user.chat_id, user.name + " joined the trip!");
    } else {
        bot_.sendMessage(user.user_id, "An error occurred during registration. Please try again later.");
    }
    return success;
}

std::optional<User> UserService::getUserDetails(long long userId, long long chatId, long long threadId) {
    return userRepository_.getUser(userId, chatId, threadId);
}

bool UserService::updateUserName(const User& user) {
    // Check if user exists before updating
    if (!userRepository_.getUser(user.user_id, user.chat_id, user.thread_id).has_value()) {
        return false;
    }
    return userRepository_.updateUser(user);
}

bool UserService::removeUser(long long userId, long long chatId, long long threadId) {
    // Check if user exists before deleting
    if (!userRepository_.getUser(userId, chatId, threadId).has_value()) {
        return false;
    }
    return userRepository_.deleteUser(userId, chatId, threadId);
}

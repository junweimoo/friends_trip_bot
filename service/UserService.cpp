#include "UserService.h"
#include <iostream>

UserService::UserService(UserRepository& userRepository, bot::Bot& bot)
    : userRepository_(userRepository), bot_(bot) {}

bool UserService::registerUser(const User& user) {
    bool success = userRepository_.registerUserWithDefaultTrip(user);
    if (success) {
        bot_.sendMessage(user.chat_id, "Successfully registered!");
    } else {
        bot_.sendMessage(user.chat_id, "An error occured during registration.");
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

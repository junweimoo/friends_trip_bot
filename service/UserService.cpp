#include "UserService.h"
#include <iostream>

UserService::UserService(UserRepository& userRepository) : userRepository_(userRepository) {}

bool UserService::registerUser(const User& user) {
    // Check if user already exists
    if (userRepository_.getUser(user.user_id, user.chat_id, user.thread_id).has_value()) {
        std::cout << "User already exists." << std::endl;
        return false;
    }
    return userRepository_.createUser(user);
}

std::optional<User> UserService::getUserDetails(long long userId, long long chatId, long long threadId) {
    return userRepository_.getUser(userId, chatId, threadId);
}

bool UserService::updateUserName(const User& user) {
    // Check if user exists before updating
    if (!userRepository_.getUser(user.user_id, user.chat_id, user.thread_id).has_value()) {
        std::cout << "User not found for update." << std::endl;
        return false;
    }
    return userRepository_.updateUser(user);
}

bool UserService::removeUser(long long userId, long long chatId, long long threadId) {
    // Check if user exists before deleting
    if (!userRepository_.getUser(userId, chatId, threadId).has_value()) {
        std::cout << "User not found for deletion." << std::endl;
        return false;
    }
    return userRepository_.deleteUser(userId, chatId, threadId);
}

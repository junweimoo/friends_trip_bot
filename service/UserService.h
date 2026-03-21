#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include "../repository/UserRepository.h"
#include "../bot/Bot.h"
#include <string>
#include <optional>

class UserService {
public:
    explicit UserService(UserRepository& userRepository, bot::Bot& bot);

    bool registerUser(const User& user);
    bool completeRegistration(const User& user);
    std::optional<User> getUserDetails(long long userId, long long chatId, long long threadId);
    bool updateUserName(const User& user);
    bool removeUser(long long userId, long long chatId, long long threadId);

private:
    UserRepository& userRepository_;
    bot::Bot& bot_;
};

#endif // USER_SERVICE_H

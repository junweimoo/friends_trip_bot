#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include "../repository/UserRepository.h"
#include <string>
#include <optional>

class UserService {
public:
    explicit UserService(UserRepository& userRepository);

    bool registerUser(const User& user);
    std::optional<User> getUserDetails(long long userId, long long chatId, long long threadId);
    bool updateUserName(const User& user);
    bool removeUser(long long userId, long long chatId, long long threadId);

private:
    UserRepository& userRepository_;
};

#endif // USER_SERVICE_H

#ifndef USER_REPOSITORY_H
#define USER_REPOSITORY_H

#include "../database/DatabaseManager.h"
#include <string>
#include <optional>
#include <vector>

struct User {
    long long user_id;
    long long chat_id;
    int thread_id;
    std::string name;
    std::string gmt_created;
    std::string gmt_modified;
};

class UserRepository {
public:
    explicit UserRepository(DatabaseManager& dbManager);

    bool createUser(const User& user);
    std::optional<User> getUser(long long userId, long long chatId, int threadId);
    bool updateUser(const User& user);
    bool deleteUser(long long userId, long long chatId, int threadId);

private:
    DatabaseManager& dbManager_;
};

#endif // USER_REPOSITORY_H

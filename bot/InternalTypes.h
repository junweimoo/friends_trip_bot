#ifndef FRIENDS_TRIP_BOT_INTERNALTYPES_H
#define FRIENDS_TRIP_BOT_INTERNALTYPES_H

#include <string>

namespace bot {

struct Message {
    long long update_id;
    long long chat_id;
    long long message_id;
    long long sender_id;
    std::string sender_name;
    std::string text;
};

struct CallbackQuery {
    long long update_id;
    long long chat_id;
    long long sender_id;
    long long message_id;
    std::string sender_name;
    std::string data;
};

}

#endif //FRIENDS_TRIP_BOT_INTERNALTYPES_H
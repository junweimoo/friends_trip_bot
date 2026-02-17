#ifndef TELEGRAM_TYPES_H
#define TELEGRAM_TYPES_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace bot {

using json = nlohmann::json;

struct Chat {
    long long id;
};

struct User {
    std::string first_name;
};

struct TelegramMessage {
    long long message_id = 0;
    Chat chat;
    std::string text;
    User from;
};

struct Update {
    long long update_id;
    TelegramMessage message;
};

struct GetUpdatesResponse {
    bool ok;
    std::vector<Update> result;
};

// JSON Deserialization Logic

inline void from_json(const json& j, Chat& c) {
    j.at("id").get_to(c.id);
}

inline void from_json(const json& j, User& u) {
    if (j.contains("first_name")) {
        j.at("first_name").get_to(u.first_name);
    }
}

inline void from_json(const json& j, TelegramMessage& m) {
    j.at("message_id").get_to(m.message_id);
    j.at("chat").get_to(m.chat);
    if (j.contains("text")) {
        j.at("text").get_to(m.text);
    }
    if (j.contains("from")) {
        j.at("from").get_to(m.from);
    }
}

inline void from_json(const json& j, Update& u) {
    j.at("update_id").get_to(u.update_id);
    if (j.contains("message")) {
        j.at("message").get_to(u.message);
    }
}

inline void from_json(const json& j, GetUpdatesResponse& r) {
    j.at("ok").get_to(r.ok);
    if (j.contains("result")) {
        j.at("result").get_to(r.result);
    }
}

}

#endif // TELEGRAM_TYPES_H

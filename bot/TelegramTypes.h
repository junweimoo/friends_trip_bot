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
    long long id;
    std::string first_name;
    std::string username;
};

struct InlineKeyboardButton {
    std::string text;
    std::string callback_data;
};

struct InlineKeyboardMarkup {
    std::vector<std::vector<InlineKeyboardButton>> inline_keyboard;
};

struct TelegramMessage {
    long long message_id = 0;
    Chat chat;
    std::string text;
    User from;
};

struct CallbackQuery {
    std::string id;
    User from;
    TelegramMessage message;
    std::string data;
};

struct Update {
    long long update_id;
    TelegramMessage message;
    CallbackQuery callback_query;
};

struct GetUpdatesResponse {
    bool ok;
    std::vector<Update> result;
};

// JSON Serialization/Deserialization Logic

inline void to_json(json& j, const InlineKeyboardButton& b) {
    j = json{{"text", b.text}, {"callback_data", b.callback_data}};
}

inline void to_json(json& j, const InlineKeyboardMarkup& k) {
    j = json{{"inline_keyboard", k.inline_keyboard}};
}

inline void from_json(const json& j, Chat& c) {
    j.at("id").get_to(c.id);
}

inline void from_json(const json& j, User& u) {
    j.at("id").get_to(u.id);
    if (j.contains("first_name")) {
        j.at("first_name").get_to(u.first_name);
    }
    if (j.contains("username")) {
        j.at("username").get_to(u.username);
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

inline void from_json(const json& j, CallbackQuery& c) {
    j.at("id").get_to(c.id);
    j.at("from").get_to(c.from);
    if (j.contains("message")) {
        j.at("message").get_to(c.message);
    }
    if (j.contains("data")) {
        j.at("data").get_to(c.data);
    }
}

inline void from_json(const json& j, Update& u) {
    j.at("update_id").get_to(u.update_id);
    if (j.contains("message")) {
        j.at("message").get_to(u.message);
    }
    if (j.contains("callback_query")) {
        j.at("callback_query").get_to(u.callback_query);
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

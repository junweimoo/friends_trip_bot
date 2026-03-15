#include "RecordPaymentConversation.h"
#include "../bot/Bot.h"
#include "../utils/MoneyAmount.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <random>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

RecordPaymentConversation::RecordPaymentConversation(long long chat_id, long long thread_id, long long user_id, bot::Bot& bot, UserRepository& userRepo, TripRepository& tripRepo, PaymentRepository& payRepo)
    : Conversation(chat_id, thread_id, user_id, bot),
      current_recipient_id(0), currentState_(State::Description), closed(false), userRepo_(userRepo), tripRepo_(tripRepo), payRepo_(payRepo) {
    // Fetch all users in the chat
    std::vector<User> chatUsers = userRepo_.getUsersByChatAndThread(chat_id, thread_id);
    for (const auto &u: chatUsers) {
        users[u.user_id] = u;
    }

    // Fetch active trip
    auto activeTrip = tripRepo_.getActiveTrip(chat_id, thread_id);
    if (activeTrip.has_value()) {
        trip = activeTrip.value();
        paymentGroup.trip_id = trip.trip_id;
    } else {
        bot.sendMessage(chat_id, "No active trip found. Please create one first.");
        closed = true;
        return;
    }

    active_message_id = bot.sendMessage(chat_id, "Enter a name for this payment");
}

RecordPaymentConversation::~RecordPaymentConversation() {
    if (!closed && active_message_id != 0) {
        bot_.editMessage(chat_id, active_message_id, "Record cancelled.");
    }
}

void RecordPaymentConversation::handleUpdate(const bot::Update& update) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed) return;

    if (update.message.message_id != 0 && update.message.text == "/cancel") {
        cancelConversation();
        return;
    }

    switch (currentState_) {
        case State::Description:
            handleDescription(update);
            break;
        case State::Amount:
            handleAmount(update);
            break;
        case State::Currency:
            handleCurrency(update);
            break;
        case State::Payer:
            handlePayer(update);
            break;
        case State::Recipient:
            handleRecipient(update);
            break;
        case State::ManualRecipient:
            handleManualRecipient(update);
            break;
        case State::ManualAmount:
            handleManualAmount(update);
            break;
        case State::EqualSplit:
            handleEqualSplitRecipients(update);
            break;
        case State::SingleRecipient:
            handleSingleRecipient(update);
            break;
    }
}

bool RecordPaymentConversation::isClosed() const {
    return closed;
}

std::string RecordPaymentConversation::createCallbackData(State targetState, const std::string& data) {
    json j;
    j["targetStep"] = static_cast<int>(targetState);
    j["data"] = data;
    return j.dump();
}

bool RecordPaymentConversation::parseCallbackData(const std::string& jsonStr, State& targetState, std::string& data) {
    try {
        auto j = json::parse(jsonStr);
        targetState = static_cast<State>(j["targetStep"].get<int>());
        data = j["data"].get<std::string>();
        return true;
    } catch (...) {
        return false;
    }
}

void RecordPaymentConversation::handleDescription(const bot::Update& update) {
    if (update.message.message_id == 0) return;

    if (update.message.text.length() > 255) {
        bot_.editMessage(chat_id, active_message_id, "Description is too long (max 255 characters). Please enter a shorter name:");
        return;
    }

    // Update name in state
    paymentGroup.name = update.message.text;

    // Update original message
    std::stringstream editedMsg;
    editedMsg << "✅ Payment Name: " << paymentGroup.name;
    bot_.editMessage(chat_id, active_message_id, editedMsg.str());

    // Ask for amount
    currentState_ = State::Amount;
    active_message_id = bot_.sendMessage(chat_id, "Enter the amount (e.g. 123 or 123.00):");
}

void RecordPaymentConversation::handleAmount(const bot::Update& update) {
    if (update.message.message_id == 0) return;

    try {
        double inputAmount = std::stod(update.message.text);
        if (inputAmount <= 0) {
            bot_.editMessage(chat_id, active_message_id, "Amount should be positive. Please enter a valid amount:");
            return;
        }
        if (std::isinf(inputAmount)) {
            bot_.editMessage(chat_id, active_message_id, "Amount is too large. Please enter a valid amount:");
            return;
        }
        pendingRawAmount_ = inputAmount;
    } catch (const std::out_of_range&) {
        bot_.editMessage(chat_id, active_message_id, "Amount is too large. Please enter a valid amount:");
        return;
    } catch (...) {
        bot_.editMessage(chat_id, active_message_id, "Invalid amount. Please enter a number (e.g. 123 or 123.00).");
        return;
    }

    // Update original message (currency not yet known, display raw number)
    std::stringstream editedMsg;
    editedMsg << "✅ Amount: " << std::fixed << std::setprecision(2) << pendingRawAmount_;
    bot_.editMessage(chat_id, active_message_id, editedMsg.str());

    // Ask for currency
    bot::InlineKeyboardMarkup keyboard;
    std::vector<std::string> currencies = {
        "USD", "EUR", "GBP",
        "JPY", "AUD", "CAD",
        "CHF", "CNY", "SEK",
        "NZD", "SGD", "MYR"};

    std::vector<bot::InlineKeyboardButton> row;
    for (const auto& curr : currencies) {
        row.push_back({curr, createCallbackData(State::Currency, curr)});
        if (row.size() == 3) {
            keyboard.inline_keyboard.push_back(row);
            row.clear();
        }
    }
    if (!row.empty()) keyboard.inline_keyboard.push_back(row);

    currentState_ = State::Currency;
    active_message_id = bot_.sendMessage(chat_id,  "Select currency:", &keyboard);
}

void RecordPaymentConversation::handleCurrency(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    State targetState;
    std::string data;
    if (!parseCallbackData(update.callback_query.data, targetState, data) || targetState != currentState_) return;

    paymentGroup.total_amount = MoneyAmount::fromMajorUnits(data, pendingRawAmount_);

    // Update original message
    std::stringstream editedMsg;
    editedMsg << "✅ Currency: " << paymentGroup.total_amount.currency();
    bot_.editMessage(chat_id, active_message_id, editedMsg.str());

    // Ask for Payer
    bot::InlineKeyboardMarkup keyboard;
    for (const auto& [uid, user] : users) {
        keyboard.inline_keyboard.push_back({{user.name, createCallbackData(State::Payer, std::to_string(uid))}});
    }

    currentState_ = State::Payer;
    active_message_id = bot_.sendMessage(chat_id,  "Who paid?", &keyboard);
}

void RecordPaymentConversation::handlePayer(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    State targetState;
    std::string data;
    if (!parseCallbackData(update.callback_query.data, targetState, data) || targetState != currentState_) return;

    try {
        paymentGroup.payer_user_id = std::stoll(data);
    } catch (...) {
        return;
    }

    // Update original message
    std::stringstream editedMsg;
    editedMsg << "✅ Payer: " << users[paymentGroup.payer_user_id].name;
    bot_.editMessage(chat_id, active_message_id, editedMsg.str());

    // Ask for Recipient
    bot::InlineKeyboardMarkup keyboard;
    keyboard.inline_keyboard.push_back({{"Split Equally", createCallbackData(State::Recipient, "split_equally")}});
    keyboard.inline_keyboard.push_back({{"Split Manually", createCallbackData(State::Recipient, "split_manually")}});
    keyboard.inline_keyboard.push_back({{"Single Recipient", createCallbackData(State::Recipient, "single_recipient")}});

    currentState_ = State::Recipient;
    active_message_id = bot_.sendMessage(chat_id,  "Who is this for?", &keyboard);
}

void RecordPaymentConversation::handleRecipient(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    State targetState;
    std::string data;
    if (!parseCallbackData(update.callback_query.data, targetState, data) || targetState != currentState_) return;

    if (data == "split_manually") {
        for (const auto& [uid, user] : users) {
            allocatedAmounts[uid] = 0;
        }
        currentState_ = State::ManualRecipient;
        sendManualRecipients(true);
    } else if (data == "split_equally") {
        // Create payment records for each user
        if (!users.empty()) {
            for (const auto& [uid, user] : users) {
                allocatedAmounts[uid] = 0;
            }
            distributeEqually();
            currentState_ = State::EqualSplit;
            sendEqualSplitRecipients(true);
        }
    } else if (data == "single_recipient") {
        currentState_ = State::SingleRecipient;
        sendSingleRecipients(true);
    }
}

void RecordPaymentConversation::sendSingleRecipients(bool editMessage) {
    std::string text = "Select the recipient:";

    bot::InlineKeyboardMarkup keyboard;
    for (const auto& [uid, user] : users) {
        keyboard.inline_keyboard.push_back({{user.name, createCallbackData(State::SingleRecipient, std::to_string(uid))}});
    }

    if (editMessage) {
        bot_.editMessage(chat_id, active_message_id, text, &keyboard);
    } else {
        active_message_id = bot_.sendMessage(chat_id, text, &keyboard);
    }
}

void RecordPaymentConversation::handleSingleRecipient(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    State targetState;
    std::string data;
    if (!parseCallbackData(update.callback_query.data, targetState, data) || targetState != currentState_) return;

    long long recipientId;
    try {
        recipientId = std::stoll(data);
    } catch (...) {
        return;
    }
    allocatedAmounts[recipientId] = paymentGroup.total_amount.minorAmount();
    completeConversation();
}

void RecordPaymentConversation::distributeEqually() {
    if (allocatedAmounts.empty()) return;
    long long total = paymentGroup.total_amount.minorAmount();
    auto n = static_cast<long long>(allocatedAmounts.size());
    long long base = total / n;
    long long remainder = total - base * n;

    // Collect keys and shuffle to randomize who gets the extra unit
    std::vector<long long> uids;
    uids.reserve(allocatedAmounts.size());
    for (const auto& [uid, amount] : allocatedAmounts) {
        uids.push_back(uid);
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(uids.begin(), uids.end(), rng);

    for (long long i = 0; i < n; ++i) {
        allocatedAmounts[uids[i]] = (i < remainder) ? base + 1 : base;
    }
}

void RecordPaymentConversation::sendEqualSplitRecipients(bool editMessage) {
    std::string text;
    if (!allocatedAmounts.empty()) {
        long long total = paymentGroup.total_amount.minorAmount();
        auto n = static_cast<long long>(allocatedAmounts.size());
        long long base = total / n;
        long long remainder = total - base * n;
        const std::string& currency = paymentGroup.total_amount.currency();
        std::stringstream ss;
        ss << "Split equally between " << n << " users";
        if (remainder > 0) {
            MoneyAmount extraAmount(currency, base + 1);
            MoneyAmount baseAmount(currency, base);

            // Group users by whether they got the extra unit
            std::vector<std::string> extraNames, baseNames;
            for (const auto& [uid, amount] : allocatedAmounts) {
                if (amount == base + 1) {
                    extraNames.push_back(users[uid].name);
                } else {
                    baseNames.push_back(users[uid].name);
                }
            }

            ss << "\n";
            for (size_t j = 0; j < extraNames.size(); ++j) {
                if (j > 0) ss << ", ";
                ss << extraNames[j];
            }
            ss << (extraNames.size() == 1 ? " gets " : " get ") << extraAmount.toHumanReadable();

            if (!baseNames.empty()) {
                ss << "\n";
                for (size_t j = 0; j < baseNames.size(); ++j) {
                    if (j > 0) ss << ", ";
                    ss << baseNames[j];
                }
                ss << (baseNames.size() == 1 ? " gets " : " get ") << baseAmount.toHumanReadable();
            }
        } else {
            MoneyAmount baseAmount(currency, base);
            ss << " (" << baseAmount.toHumanReadable() << " each)";
        }
        text = ss.str();
    } else {
        text = "No users selected";
    }

    bot::InlineKeyboardMarkup keyboard;
    if (!allocatedAmounts.empty()) {
        keyboard.inline_keyboard.push_back({{"Done", createCallbackData(State::EqualSplit, "done")}});
    }
    keyboard.inline_keyboard.push_back({{"Select all", createCallbackData(State::EqualSplit, "select_all")}});
    keyboard.inline_keyboard.push_back({{"Unselect all", createCallbackData(State::EqualSplit, "unselect_all")}});
    for (const auto& [uid, user] : users) {
        std::stringstream btnText;
        if (allocatedAmounts.find(uid) != allocatedAmounts.end()) {
            btnText << "✅ " <<  user.name;
        } else {
            btnText << user.name;
        }
        keyboard.inline_keyboard.push_back({{btnText.str(), createCallbackData(State::EqualSplit, std::to_string(uid))}});
    }

    if (editMessage) {
        bot_.editMessage(chat_id, active_message_id, text, &keyboard);
    } else {
        active_message_id = bot_.sendMessage(chat_id, text, &keyboard);
    }
}

void RecordPaymentConversation::handleEqualSplitRecipients(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    State targetState;
    std::string data;
    if (!parseCallbackData(update.callback_query.data, targetState, data) || targetState != currentState_) return;

    if (data == "done") {
        completeConversation();
        return;
    }

    if (data == "randomize") {
        distributeEqually();
        sendEqualSplitRecipients(true);
        return;
    }

    if (data == "select_all") {
        if (!users.empty()) {
            for (const auto& [uid, user] : users) {
                allocatedAmounts[uid] = 0;
            }
            distributeEqually();
        }
    } else if (data == "unselect_all") {
        allocatedAmounts.clear();
    } else {
        long long recipientId;
        try {
            recipientId = std::stoll(data);
        } catch (...) {
            return;
        }
        if (allocatedAmounts.find(recipientId) != allocatedAmounts.end()) {
            allocatedAmounts.erase(recipientId);
        } else {
            allocatedAmounts[recipientId] = 0;
        }
        distributeEqually();
    }
    sendEqualSplitRecipients(true);
}

void RecordPaymentConversation::sendManualRecipients(bool editMessage) {
    long long currentAllocated = 0;
    for (const auto& [uid, amount] : allocatedAmounts) {
        currentAllocated += amount;
    }

    const std::string& currency = paymentGroup.total_amount.currency();
    MoneyAmount allocated(currency, currentAllocated);
    MoneyAmount remaining(currency, paymentGroup.total_amount.minorAmount() - currentAllocated);

    std::stringstream ss;
    ss << "Allocated " << allocated.toHumanReadable() << "/" << paymentGroup.total_amount.toHumanReadable()
       << " (" << remaining.toHumanReadable() << " remaining)";
    std::string text = ss.str();

    bot::InlineKeyboardMarkup keyboard;
    if (currentAllocated == paymentGroup.total_amount.minorAmount()) {
        keyboard.inline_keyboard.push_back({{"Done", createCallbackData(State::ManualRecipient, "done")}});
    }

    for (const auto& [uid, user] : users) {
        std::stringstream btnText;
        btnText << user.name << " (" << MoneyAmount(currency, allocatedAmounts[uid]).toHumanReadable() << ")";
        keyboard.inline_keyboard.push_back({{btnText.str(), createCallbackData(State::ManualRecipient, std::to_string(uid))}});
    }

    if (editMessage) {
        bot_.editMessage(chat_id, active_message_id, text, &keyboard);
    } else {
        active_message_id = bot_.sendMessage(chat_id, text, &keyboard);
    }
}

void RecordPaymentConversation::handleManualRecipient(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    State targetState;
    std::string data;
    if (!parseCallbackData(update.callback_query.data, targetState, data) || targetState != currentState_) return;

    if (data == "done") {
        // Check if total allocated matches total amount
        long long currentAllocated = 0;
        for (const auto& [uid, amount] : allocatedAmounts) {
            currentAllocated += amount;
        }

        if (currentAllocated != paymentGroup.total_amount.minorAmount()) {
            bot_.sendMessage(chat_id, "Allocated amount does not match total amount. Please adjust.");
            sendManualRecipients(true);
            return;
        }

        completeConversation();
        return;
    }

    try {
        current_recipient_id = std::stoll(data);
    } catch (...) {
        // Handle error
    }
    currentState_ = State::ManualAmount;
    bot_.editMessage(chat_id, active_message_id, "Enter amount for " + users[current_recipient_id].name + " (e.g. 123 or 123.00):");
}

void RecordPaymentConversation::handleManualAmount(const bot::Update& update) {
    if (update.message.message_id == 0) return;

    try {
        double inputAmount = std::stod(update.message.text);
        if (inputAmount < 0) {
            bot_.editMessage(chat_id, active_message_id, "Amount cannot be negative. Please enter a valid amount:");
            return;
        }
        if (std::isinf(inputAmount)) {
            bot_.editMessage(chat_id, active_message_id, "Amount is too large. Please enter a valid amount:");
            return;
        }
        long long minorAmount = MoneyAmount::fromMajorUnits(paymentGroup.total_amount.currency(), inputAmount).minorAmount();
        allocatedAmounts[current_recipient_id] = minorAmount;

        currentState_ = State::ManualRecipient;
        sendManualRecipients(true);
    } catch (const std::out_of_range&) {
        bot_.editMessage(chat_id, active_message_id, "Amount is too large. Please enter a valid amount:");
    } catch (...) {
        bot_.editMessage(chat_id, active_message_id, "Invalid amount. Please enter a number.");
    }
}

void RecordPaymentConversation::completeConversation() {
    paymentGroup.records.clear();

    std::chrono::system_clock::time_point current_time{};
    const std::string& currency = paymentGroup.total_amount.currency();

    // Create records from allocatedAmounts
    for (const auto& [uid, amount] : allocatedAmounts) {
        if (amount > 0) {
            paymentGroup.records.push_back({
                0, 0, paymentGroup.trip_id,
                MoneyAmount(currency, amount),
                paymentGroup.payer_user_id, uid, current_time
            });
        }
    }

    std::stringstream overviewMsg;
    overviewMsg << "<b>👍 Payment recorded!</b>\n";
    overviewMsg << "<b>Name:</b> " << paymentGroup.name << "\n";
    overviewMsg << "<b>Amount:</b> " << paymentGroup.total_amount.toHumanReadable() << "\n";
    overviewMsg << "<b>Payer:</b> " << users[paymentGroup.payer_user_id].name << "\n";
    overviewMsg << "<b>Recipients:</b>\n";

    for (const auto& [uid, amount] : allocatedAmounts) {
        if (amount > 0) {
            overviewMsg << "- " << users[uid].name << " ("
                        << MoneyAmount(currency, amount).toHumanReadable() << ")\n";
        }
    }

    if (payRepo_.createPaymentGroup(paymentGroup)) {
        bot_.editMessage(chat_id, active_message_id, overviewMsg.str(), nullptr, "HTML");
        spdlog::info("Payment group created successfully: {}", paymentGroup.name);
    } else {
        bot_.editMessage(chat_id, active_message_id, "Failed to record payment. Please try again.");
    }
    closed = true;
}

void RecordPaymentConversation::cancelConversation() {
    bot_.sendMessage(chat_id, "Record cancelled.");
    closed = true;
}

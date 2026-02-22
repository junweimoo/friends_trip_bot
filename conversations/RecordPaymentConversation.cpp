#include "RecordPaymentConversation.h"
#include "../repository/UserRepository.h"
#include "../repository/TripRepository.h"
#include "../bot/Bot.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <spdlog/spdlog.h>

RecordPaymentConversation::RecordPaymentConversation(long long chat_id, long long thread_id, long long user_id, bot::Bot& bot, UserRepository& userRepo, TripRepository& tripRepo, PaymentRepository& payRepo)
    : Conversation(chat_id, user_id, bot), chat_id(chat_id), thread_id(thread_id), user_id(user_id), step(0), closed(false), userRepo_(userRepo), tripRepo_(tripRepo), payRepo_(payRepo) {

    // Fetch all users in the chat
    std::vector<User> chatUsers = userRepo_.getUsersByChatAndThread(chat_id, thread_id);
    for (const auto& u : chatUsers) {
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

    active_message_id = bot.sendMessage(chat_id,"Enter a name for this payment");
}

RecordPaymentConversation::~RecordPaymentConversation() {}

void RecordPaymentConversation::handleUpdate(const bot::Update& update) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed) return;

    if (update.message.message_id != 0 && update.message.text == "/cancel") {
        cancelConversation();
        return;
    }

    switch (step) {
        case 0:
            handleDescription(update);
            break;
        case 1:
            handleAmount(update);
            break;
        case 2:
            handleCurrency(update);
            break;
        case 3:
            handlePayer(update);
            break;
        case 4:
            handleRecipient(update);
            break;
        case 5:
            handleManualRecipient(update);
            break;
        case 6:
            handleManualAmount(update);
            break;
        case 7:
            handleEqualSplitRecipients(update);
            break;
        case 8:
            handleSingleRecipient(update);
            break;
        default:
            break;
    }
}

bool RecordPaymentConversation::isClosed() const {
    return closed;
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
    step = 1;
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
        paymentGroup.total_amount = std::round(inputAmount * 100.0) / 100.0;
    } catch (const std::out_of_range&) {
        bot_.editMessage(chat_id, active_message_id, "Amount is too large. Please enter a valid amount:");
        return;
    } catch (...) {
        bot_.editMessage(chat_id, active_message_id, "Invalid amount. Please enter a number (e.g. 123 or 123.00).");
        return;
    }

    // Update original message
    std::stringstream editedMsg;
    editedMsg << "✅ Amount: " << std::fixed << std::setprecision(2) << paymentGroup.total_amount;
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
        row.push_back({curr, curr});
        if (row.size() == 3) {
            keyboard.inline_keyboard.push_back(row);
            row.clear();
        }
    }
    if (!row.empty()) keyboard.inline_keyboard.push_back(row);

    step = 2;
    active_message_id = bot_.sendMessage(chat_id,  "Select currency:", &keyboard);
}

void RecordPaymentConversation::handleCurrency(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    paymentGroup.currency = update.callback_query.data;

    // Update original message
    std::stringstream editedMsg;
    editedMsg << "✅ Currency: " << paymentGroup.currency;
    bot_.editMessage(chat_id, active_message_id, editedMsg.str());

    // Ask for Payer
    bot::InlineKeyboardMarkup keyboard;
    for (const auto& [uid, user] : users) {
        keyboard.inline_keyboard.push_back({{user.name, std::to_string(uid)}});
    }

    step = 3;
    active_message_id = bot_.sendMessage(chat_id,  "Who paid?", &keyboard);
}

void RecordPaymentConversation::handlePayer(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    try {
        paymentGroup.payer_user_id = std::stoll(update.callback_query.data);
    } catch (...) {
        return;
    }

    // Update original message
    std::stringstream editedMsg;
    editedMsg << "✅ Payer: " << users[paymentGroup.payer_user_id].name;
    bot_.editMessage(chat_id, active_message_id, editedMsg.str());

    // Ask for Recipient
    bot::InlineKeyboardMarkup keyboard;
    keyboard.inline_keyboard.push_back({{"Split Equally", "split_equally"}});
    keyboard.inline_keyboard.push_back({{"Split Manually", "split_manually"}});
    keyboard.inline_keyboard.push_back({{"Single Recipient", "single_recipient"}});

    step = 4;
    active_message_id = bot_.sendMessage(chat_id,  "Who is this for?", &keyboard);
}

void RecordPaymentConversation::handleRecipient(const bot::Update& update) {
    if (update.callback_query.id.empty()) return;
    bot_.answerCallbackQuery(update.callback_query.id);

    std::string data = update.callback_query.data;

    if (data == "split_manually") {
        for (const auto& [uid, user] : users) {
            allocatedAmounts[uid] = 0.0;
        }
        step = 5;
        sendManualRecipients(true);
    } else if (data == "split_equally") {
        // Create payment records for each user
        if (!users.empty()) {
            double splitAmount = std::round((paymentGroup.total_amount / users.size()) * 100.0) / 100.0;
            for (const auto& [uid, user] : users) {
                allocatedAmounts[uid] = splitAmount;
            }
            step = 7;
            sendEqualSplitRecipients(true);
        }
    } else if (data == "single_recipient") {
        step = 8;
        sendSingleRecipients(true);
    }
}

void RecordPaymentConversation::sendSingleRecipients(bool editMessage) {
    std::string text = "Select the recipient:";

    bot::InlineKeyboardMarkup keyboard;
    for (const auto& [uid, user] : users) {
        keyboard.inline_keyboard.push_back({{user.name, std::to_string(uid)}});
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

    long long recipientId;
    try {
        recipientId = std::stoll(update.callback_query.data);
    } catch (...) {
        return;
    }
    allocatedAmounts[recipientId] = paymentGroup.total_amount;
    completeConversation();
}

void RecordPaymentConversation::sendEqualSplitRecipients(bool editMessage) {
    double splitAmount;
    std::string text;
    if (!allocatedAmounts.empty()) {
        splitAmount = std::round((paymentGroup.total_amount / allocatedAmounts.size()) * 100.0) / 100.0;
        std::stringstream ss;
        ss << "Split equally between " << allocatedAmounts.size() << " users (" << std::fixed << std::setprecision(2)
           << splitAmount << " each)";
        text = ss.str();
    } else {
        text = "No users selected";
    }

    bot::InlineKeyboardMarkup keyboard;
    if (!allocatedAmounts.empty()) {
        keyboard.inline_keyboard.push_back({{"Done", "done"}});
    }
    keyboard.inline_keyboard.push_back({{"Select all", "select_all"}});
    keyboard.inline_keyboard.push_back({{"Unselect all", "unselect_all"}});
    for (const auto& [uid, user] : users) {
        std::stringstream btnText;
        if (allocatedAmounts.find(uid) != allocatedAmounts.end()) {
            btnText << "✅ " <<  user.name;
        } else {
            btnText << user.name;
        }
        keyboard.inline_keyboard.push_back({{btnText.str(), std::to_string(uid)}});
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

    std::string data = update.callback_query.data;
    if (data == "done") {
        completeConversation();
        return;
    }

    if (data == "select_all") {
        // Add all recipients to allocatedAmounts
        if (!users.empty()) {
            double splitAmount = std::round((paymentGroup.total_amount / users.size()) * 100.0) / 100.0;
            double roundedTotalAmount = splitAmount * users.size();
            for (const auto& [uid, user] : users) {
                allocatedAmounts[uid] = splitAmount;
            }
        }
    } else if (data == "unselect_all") {
        // Clear all recipients from allocatedAmounts
        allocatedAmounts.clear();
    } else {
        long long recipientId;
        try {
            recipientId = std::stoll(data);
        } catch (...) {
            return;
        }
        if (allocatedAmounts.find(recipientId) != allocatedAmounts.end()) {
            // Remove recipient from allocatedAmounts
            allocatedAmounts.erase(recipientId);
            if (!allocatedAmounts.empty()) {
                double splitAmount = std::round((paymentGroup.total_amount / allocatedAmounts.size()) * 100.0) / 100.0;
                for (auto& [uid, amount] : allocatedAmounts) {
                    amount = splitAmount;
                }
            }
        } else {
            // Add recipient to allocatedAmounts
            double splitAmount = std::round((paymentGroup.total_amount / (allocatedAmounts.size() + 1)) * 100.0) / 100.0;
            allocatedAmounts[recipientId] = splitAmount;
            for (auto& [uid, amount] : allocatedAmounts) {
                amount = splitAmount;
            }
        }
    }
    sendEqualSplitRecipients(true);
}

void RecordPaymentConversation::sendManualRecipients(bool editMessage) {
    double currentAllocated = 0;
    for (const auto& [uid, amount] : allocatedAmounts) {
        currentAllocated += amount;
    }

    std::stringstream ss;
    ss << "Allocated " << std::fixed << std::setprecision(2) << currentAllocated << "/" << paymentGroup.total_amount
       << " (" << std::fixed << std::setprecision(2) << (paymentGroup.total_amount - currentAllocated) << " remaining)";
    std::string text = ss.str();

    bot::InlineKeyboardMarkup keyboard;
    if (std::abs(currentAllocated - paymentGroup.total_amount) < 0.01) {
        keyboard.inline_keyboard.push_back({{"Done", "done"}});
    }

    for (const auto& [uid, user] : users) {
        std::stringstream btnText;
        btnText << user.name << " (" << std::fixed << std::setprecision(2) << allocatedAmounts[uid] << ")";
        keyboard.inline_keyboard.push_back({{btnText.str(), std::to_string(uid)}});
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

    std::string data = update.callback_query.data;

    if (data == "done") {
        // Check if total allocated matches total amount
        double currentAllocated = 0;
        for (const auto& [uid, amount] : allocatedAmounts) {
            currentAllocated += amount;
        }

        if (std::abs(currentAllocated - paymentGroup.total_amount) > 0.01) {
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
    step = 6;
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
        double roundedAmount = std::round(inputAmount * 100.0) / 100.0;
        allocatedAmounts[current_recipient_id] = roundedAmount;

        step = 5;
        sendManualRecipients(true);
    } catch (const std::out_of_range&) {
        bot_.editMessage(chat_id, active_message_id, "Amount is too large. Please enter a valid amount:");
    } catch (...) {
        bot_.editMessage(chat_id, active_message_id, "Invalid amount. Please enter a number.");
    }
}

void RecordPaymentConversation::completeConversation() {
    paymentGroup.records.clear();

    // Create records from allocatedAmounts
    for (const auto& [uid, amount] : allocatedAmounts) {
        if (amount > 0) {
            paymentGroup.records.push_back({
                0, 0, paymentGroup.trip_id, amount, paymentGroup.currency, paymentGroup.payer_user_id, uid, ""
            });
        }
    }

    std::stringstream overviewMsg;
    overviewMsg << "<b>👍 Payment recorded!</b>\n";
    overviewMsg << "<b>Name:</b> " << paymentGroup.name << "\n";
    overviewMsg << "<b>Amount:</b> " << std::fixed << std::setprecision(2) << paymentGroup.total_amount << " " << paymentGroup.currency << "\n";
    overviewMsg << "<b>Payer:</b> " << users[paymentGroup.payer_user_id].name << "\n";
    overviewMsg << "<b>Recipients:</b>\n";

    for (const auto& [uid, amount] : allocatedAmounts) {
        if (amount > 0) {
            overviewMsg << "- " << users[uid].name << " ("
                        << std::fixed << std::setprecision(2) << amount << " "
                        << paymentGroup.currency << ")\n";
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
    bot_.sendMessage(chat_id, "Conversation cancelled.");
    closed = true;
}

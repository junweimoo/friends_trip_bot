#include "ListPaymentsConversation.h"
#include "../bot/Bot.h"
#include "../utils/utils.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <chrono>

ListPaymentsConversation::ListPaymentsConversation(long long chat_id, long long thread_id, long long user_id, bot::Bot& bot, UserRepository& userRepo, TripRepository& tripRepo, PaymentRepository& payRepo)
    : Conversation(chat_id, thread_id, user_id, bot), pageSize(10), currentPage(1), closed(false), active_message_id(0), userRepo_(userRepo), tripRepo_(tripRepo), payRepo_(payRepo) {

    // Fetch active trip
    auto activeTrip = tripRepo_.getActiveTrip(chat_id, thread_id);
    if (activeTrip.has_value()) {
        trip = activeTrip.value();
    } else {
        bot.sendMessage(chat_id, "No active trip found.");
        closed = true;
        return;
    }

    // Fetch all payment groups
    paymentGroups = payRepo_.getAllPaymentGroups(trip.trip_id);

    // Fetch all users
    std::vector<User> chatUsers = userRepo_.getUsersByChatAndThread(chat_id, thread_id);
    for (const auto& u : chatUsers) {
        users[u.user_id] = u;
    }

    // Calculate total pages
    totalPages = (paymentGroups.empty()) ? 1 : std::ceil(static_cast<double>(paymentGroups.size()) / pageSize);

    computeNetBalances();
    sendCurrentPage(false);
}

ListPaymentsConversation::~ListPaymentsConversation() {
    if (!closed && active_message_id != 0) {
        bot_.editMessage(chat_id, active_message_id, "List closed.");
    }
}

void ListPaymentsConversation::handleUpdate(const bot::Update& update) {
    if (closed) return;

    if (update.message.message_id != 0 && update.message.text == "/cancel") {
        closeConversation();
        return;
    }

    if (!update.callback_query.id.empty()) {
        if (update.callback_query.data == "close") {
            bot_.answerCallbackQuery(update.callback_query.id);
            closeConversation();
            return;
        }
        handlePageChange(update);
    }
}

bool ListPaymentsConversation::isClosed() const {
    return closed;
}

void ListPaymentsConversation::computeNetBalances() {
    netBalances.clear();
    for (const auto& group : paymentGroups) {
        // Payer paid total_amount
        netBalances[group.payer_user_id][group.currency] += group.total_amount;

        // Each record recipient "received" (owes) amount
        for (const auto& record : group.records) {
            netBalances[record.to_user_id][group.currency] -= record.amount;
        }
    }
}

void ListPaymentsConversation::sendCurrentPage(bool editMessage) {
    std::stringstream ss;
    ss << "<b>Payments in " << trip.name << "</b>\n\n";

    // Print Net Balances
    ss << "<b>Net Balances:</b>\n";
    if (netBalances.empty()) {
        ss << "No balances.\n";
    } else {
        for (const auto& [userId, balances] : netBalances) {
            ss << "👤 <b>" << users[userId].name << ":</b>\n";
            for (const auto& [currency, amount] : balances) {
                std::string color = (amount >= 0) ? "🟢" : "🔴";
                ss << "  " << color << " " << std::fixed << std::setprecision(2) << amount << " " << currency << "\n";
            }
        }
    }
    ss << "\n";

    ss << "<b>Payment List (Page " << currentPage << "/" << totalPages << "):</b>\n";

    int startIdx = (currentPage - 1) * pageSize;
    int endIdx = std::min(startIdx + pageSize, static_cast<int>(paymentGroups.size()));

    if (paymentGroups.empty()) {
        ss << "No payments recorded yet.";
    } else {
        for (int i = startIdx; i < endIdx; ++i) {
            const auto& group = paymentGroups[i];

            ss << "📂 " << i + 1 << ". <b>" << group.name << "</b> (" << utils::formatTimestamp(group.gmt_created, 8) << ")\n";
            ss << "<b>" << std::fixed << std::setprecision(2) << group.total_amount << " " << group.currency << "</b> paid by <b>" << users[group.payer_user_id].name << "</b>\n";

            for (const auto& record : group.records) {
                ss << "- " << users[record.to_user_id].name << " received " << std::fixed << std::setprecision(2) << record.amount << " " << record.currency << "\n";
            }
            ss << "\n";
        }
    }

    bot::InlineKeyboardMarkup keyboard;
    std::vector<bot::InlineKeyboardButton> row;

    if (currentPage > 1) {
        row.push_back({"Previous page", "prev_page"});
    }
    if (currentPage < totalPages) {
        row.push_back({"Next page", "next_page"});
    }
    if (!row.empty()) {
        keyboard.inline_keyboard.push_back(row);
    }
    keyboard.inline_keyboard.push_back({{"Close", "close"}});

    if (editMessage && active_message_id != 0) {
        bot_.editMessage(trip.chat_id, active_message_id, ss.str(), &keyboard, "HTML");
    } else {
        active_message_id = bot_.sendMessage(trip.chat_id, ss.str(), &keyboard, "HTML");
    }
}

void ListPaymentsConversation::handlePageChange(const bot::Update& update) {
    bot_.answerCallbackQuery(update.callback_query.id);

    std::string data = update.callback_query.data;
    if (data == "prev_page" && currentPage > 1) {
        currentPage--;
        sendCurrentPage(true);
    } else if (data == "next_page" && currentPage < totalPages) {
        currentPage++;
        sendCurrentPage(true);
    }
}


void ListPaymentsConversation::closeConversation() {
    if (active_message_id != 0) {
        bot_.editMessage(chat_id, active_message_id, "List closed.");
    }
    closed = true;
}

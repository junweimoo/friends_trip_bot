#include "SimplifyPaymentsConversation.h"
#include "../algorithm/GreedyDebtSimplifier.h"
#include "../algorithm/MinTransactionsSimplifier.h"
#include "../bot/Bot.h"
#include "../service/PaymentService.h"
#include <memory>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <set>
#include <map>

SimplifyPaymentsConversation::SimplifyPaymentsConversation(long long chat_id, long long thread_id, long long user_id,
    bot::Bot& bot, UserRepository& userRepo, TripRepository& tripRepo, PaymentRepository& payRepo,
    PaymentService& paymentService)
    : Conversation(chat_id, thread_id, user_id, bot),
      currentState_(State::SelectingCurrency), closed_(false), active_message_id_(0),
      userRepo_(userRepo), tripRepo_(tripRepo), payRepo_(payRepo), paymentService_(paymentService),
      currentForeignCurrencyIndex_(0) {

    auto activeTrip = tripRepo_.getActiveTrip(chat_id, thread_id);
    if (activeTrip.has_value()) {
        trip_ = activeTrip.value();
    } else {
        bot_.sendMessage(chat_id, "No active trip found.");
        closed_ = true;
        return;
    }

    paymentGroups_ = payRepo_.getAllPaymentGroups(trip_.trip_id);
    if (paymentGroups_.empty()) {
        bot_.sendMessage(chat_id, "No payments recorded yet.");
        closed_ = true;
        return;
    }

    std::vector<User> chatUsers = userRepo_.getUsersByChatAndThread(chat_id, thread_id);
    for (const auto& u : chatUsers) {
        users_[u.user_id] = u;
    }

    sendCurrencySelection(false);
}

SimplifyPaymentsConversation::~SimplifyPaymentsConversation() {
    if (!closed_ && active_message_id_ != 0) {
        bot_.editMessage(chat_id, active_message_id_, "Simplify closed.");
    }
}

void SimplifyPaymentsConversation::handleUpdate(const bot::Update& update) {
    if (closed_) return;

    if (update.message.message_id != 0 && update.message.text == "/cancel") {
        closeConversation();
        return;
    }

    switch (currentState_) {
        case State::SelectingCurrency:
            if (!update.callback_query.id.empty()) {
                handleCurrencySelection(update);
            }
            break;

        case State::CollectingExchangeRate:
            if (update.message.message_id != 0) {
                handleExchangeRateInput(update);
            }
            break;
    }
}

bool SimplifyPaymentsConversation::isClosed() const {
    return closed_;
}

void SimplifyPaymentsConversation::sendCurrencySelection(bool edit) {
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

    if (edit && active_message_id_ != 0) {
        bot_.editMessage(chat_id, active_message_id_, "Select the settlement currency:", &keyboard);
    } else {
        active_message_id_ = bot_.sendMessage(chat_id, "Select the settlement currency:", &keyboard);
    }
}

void SimplifyPaymentsConversation::handleCurrencySelection(const bot::Update& update) {
    bot_.answerCallbackQuery(update.callback_query.id);
    targetCurrency_ = update.callback_query.data;
    exchangeRates_[targetCurrency_] = 1.0;

    // Collect distinct foreign currencies from all payment groups
    std::set<std::string> seen;
    for (const auto& group : paymentGroups_) {
        if (group.total_amount.currency() != targetCurrency_) {
            seen.insert(group.total_amount.currency());
        }
    }
    foreignCurrencies_.assign(seen.begin(), seen.end());

    std::stringstream confirmed;
    confirmed << "✅ Target currency: " << targetCurrency_;
    bot_.editMessage(chat_id, active_message_id_, confirmed.str());

    if (foreignCurrencies_.empty()) {
        computeAndDisplayResults();
    } else {
        currentForeignCurrencyIndex_ = 0;
        currentState_ = State::CollectingExchangeRate;
        sendExchangeRatePrompt(false);
    }
}

void SimplifyPaymentsConversation::sendExchangeRatePrompt(bool edit) {
    const std::string& foreign = foreignCurrencies_[currentForeignCurrencyIndex_];
    std::string text = "Enter exchange rate: 1 " + foreign + " = ? " + targetCurrency_;

    if (edit && active_message_id_ != 0) {
        bot_.editMessage(chat_id, active_message_id_, text);
    } else {
        active_message_id_ = bot_.sendMessage(chat_id, text);
    }
}

void SimplifyPaymentsConversation::handleExchangeRateInput(const bot::Update& update) {
    const std::string& text = update.message.text;

    double rate;
    try {
        rate = std::stod(text);
    } catch (...) {
        bot_.sendMessage(chat_id, "Invalid number. Please enter a valid exchange rate.");
        return;
    }

    if (rate <= 0 || std::isinf(rate)) {
        bot_.sendMessage(chat_id, "Exchange rate must be a positive number. Please try again.");
        return;
    }

    const std::string& foreign = foreignCurrencies_[currentForeignCurrencyIndex_];
    exchangeRates_[foreign] = rate;

    // Edit the prompt message to show confirmed rate
    std::stringstream confirmed;
    confirmed << "✅ 1 " << foreign << " = " << std::fixed << std::setprecision(12) << rate << " " << targetCurrency_;
    if (active_message_id_ != 0) {
        bot_.editMessage(chat_id, active_message_id_, confirmed.str());
    }

    currentForeignCurrencyIndex_++;

    if (currentForeignCurrencyIndex_ < foreignCurrencies_.size()) {
        sendExchangeRatePrompt(false);
    } else {
        computeAndDisplayResults();
    }
}

void SimplifyPaymentsConversation::computeAndDisplayResults() {
    std::set<long long> participants;
    for (const auto& group : paymentGroups_) {
        for (const auto& record : group.records) {
            participants.insert(record.from_user_id);
            participants.insert(record.to_user_id);
        }
    }

    static constexpr size_t MIN_TRANSACTIONS_THRESHOLD = 0;
    std::unique_ptr<DebtSimplifier> simplifier;
    if (participants.size() <= MIN_TRANSACTIONS_THRESHOLD) {
        simplifier = std::make_unique<MinTransactionsSimplifier>();
    } else {
        simplifier = std::make_unique<GreedyDebtSimplifier>();
    }

    auto simplifiedPayments = simplifier->simplifyDebts(paymentGroups_, exchangeRates_, targetCurrency_);

    std::stringstream ss;
    ss << "💰 <b>Simplified Payments (" << targetCurrency_ << ")</b>\n";
    ss << "🧳 <b>Trip: " << trip_.name << "</b>\n\n";

    if (simplifiedPayments.empty()) {
        ss << "✅ All balances are settled! No payments needed.";
    } else {
        for (const auto& payment : simplifiedPayments) {
            ss << "➡️ <b>" << users_[payment.from_user_id].name << "</b> pays "
               << "<b>" << users_[payment.to_user_id].name << "</b>: "
               << "<b>" << payment.amount.toHumanReadable() << "</b>\n";
        }
        ss << "\n✅ <b>" << simplifiedPayments.size() << "</b> transaction"
           << (simplifiedPayments.size() > 1 ? "s" : "") << " to settle all debts"
           << "\n\n📬 DMs have been sent to users who owe money";
    }

    bot_.sendMessage(chat_id, ss.str(), nullptr, "HTML");

    // Send a private message per payment with a "Log Payment" button
    if (!simplifiedPayments.empty()) {
        for (const auto& payment : simplifiedPayments) {
            std::stringstream dm;
            dm << "💰 <b>Trip: " << trip_.name << "</b>\n\n"
               << "➡️ Pay <b>" << users_[payment.to_user_id].name << "</b>: "
               << "<b>" << payment.amount.toHumanReadable() << "</b>";

            // Build PaymentGroup with pre-stored parameters
            PaymentGroup paymentGroup;
            paymentGroup.payment_group_id = 0;
            paymentGroup.trip_id = trip_.trip_id;
            paymentGroup.name = "Simplified Repayment";
            paymentGroup.total_amount = payment.amount;
            paymentGroup.payer_user_id = payment.from_user_id;
            paymentGroup.gmt_created = {};

            PaymentRecord record;
            record.payment_record_id = 0;
            record.payment_group_id = 0;
            record.trip_id = trip_.trip_id;
            record.amount = payment.amount;
            record.from_user_id = payment.from_user_id;
            record.to_user_id = payment.to_user_id;
            record.gmt_created = {};
            paymentGroup.records.push_back(record);

            std::string fromName = users_[payment.from_user_id].name;
            std::string toName = users_[payment.to_user_id].name;

            std::string key = bot_.storeCallback(
                [&paymentService = paymentService_, paymentGroup, fromName, toName, groupChatId = chat_id]() {
                    paymentService.logSimplifiedPayment(paymentGroup, fromName, toName, groupChatId);
                });

            bot::InlineKeyboardMarkup keyboard;
            keyboard.inline_keyboard.push_back(
                {{"\xe2\x9c\x85 Log Payment", key}});

            bot_.sendMessage(payment.from_user_id, dm.str(), &keyboard, "HTML", "lp");
        }
    }

    closeConversation();
}

void SimplifyPaymentsConversation::closeConversation() {
    closed_ = true;
}
#include "TripsConversation.h"
#include <sstream>

TripsConversation::TripsConversation(long long chat_id, long long user_id, bot::Bot& bot, TripRepository& tripRepo)
    : Conversation(chat_id, 0, user_id, bot), bot_(bot), tripRepo_(tripRepo),
      currentState_(State::ShowingTrips), message_id_(0), closed_(false) {

    allTrips_ = tripRepo_.getAllTrips(chat_id, 0);
    activeTrip_ = tripRepo_.getActiveTrip(chat_id, 0); // Assuming thread_id is 0 for chat-wide trips
    sendTripList(false);
}

TripsConversation::~TripsConversation() {
    if (message_id_ != 0) {
        bot_.editMessage(chat_id, message_id_, "Trip selection closed.");
    }
}

void TripsConversation::handleUpdate(const bot::Update& update) {
    if (closed_) return;

    if (update.callback_query.id != "") {
        handleCallback(update);
    } else if (update.message.message_id != 0) {
        if (currentState_ == State::CreatingTrip) {
            handleNewTripName(update);
        }
    }
}

bool TripsConversation::isClosed() const {
    return closed_;
}

void TripsConversation::sendTripList(bool edit) {
    std::stringstream ss;
    ss << "Please select a trip or create a new one:";

    bot::InlineKeyboardMarkup keyboard;
    keyboard.inline_keyboard.push_back({{"➕ New Trip", "new_trip"}});

    for (const auto& trip : allTrips_) {
        std::string buttonText = trip.name;
        if (activeTrip_ && activeTrip_->trip_id == trip.trip_id) {
            buttonText = "✅ " + buttonText;
        }
        keyboard.inline_keyboard.push_back({{buttonText, "trip_" + std::to_string(trip.trip_id)}});
    }

    if (allTrips_.size() >= 2) {
        keyboard.inline_keyboard.push_back({{"🗑️ Delete current trip", "delete_trip"}});
    }
    keyboard.inline_keyboard.push_back({{"Close", "close"}});

    if (edit) {
        bot_.editMessage(chat_id, message_id_, ss.str(), &keyboard);
    } else {
        message_id_ = bot_.sendMessage(chat_id, ss.str(), &keyboard);
    }
}

void TripsConversation::handleCallback(const bot::Update& update) {
    bot_.answerCallbackQuery(update.callback_query.id);
    const std::string& data = update.callback_query.data;

    if (currentState_ == State::ConfirmingDelete) {
        handleDeleteConfirm(update);
        return;
    }

    if (data == "new_trip") {
        currentState_ = State::CreatingTrip;
        bot_.editMessage(chat_id, message_id_, "Please enter the name for the new trip:");
    } else if (data.rfind("trip_", 0) == 0) {
        long long tripId = std::stoll(data.substr(5));
        if (tripRepo_.updateActiveTrip(chat_id, 0, tripId)) {
             activeTrip_ = tripRepo_.getActiveTrip(chat_id, 0);
             allTrips_ = tripRepo_.getAllTrips(chat_id, 0); // Refresh trips
             sendTripList(true);
        }
    } else if (data == "delete_trip") {
        if (activeTrip_ && allTrips_.size() >= 2) {
            currentState_ = State::ConfirmingDelete;
            bot::InlineKeyboardMarkup keyboard;
            keyboard.inline_keyboard.push_back({{"Yes, delete", "confirm_delete"}, {"Cancel", "cancel_delete"}});
            bot_.editMessage(chat_id, message_id_,
                "Are you sure you want to delete the current trip \"" + activeTrip_->name + "\"?",
                &keyboard);
        }
    } else if (data == "close") {
        closeConversation();
    }
}

void TripsConversation::handleNewTripName(const bot::Update& update) {
    const std::string& newTripName = update.message.text;
    if (newTripName.empty()) {
        return;
    }
    if (long long newTripId = tripRepo_.createTrip(chat_id, 0, newTripName); newTripId != -1) {
        if (tripRepo_.updateActiveTrip(chat_id, 0, newTripId)) {
            activeTrip_ = tripRepo_.getActiveTrip(chat_id, 0);
            allTrips_ = tripRepo_.getAllTrips(chat_id, 0);
            currentState_ = State::ShowingTrips;
            sendTripList(true);
        }
    } else {
        bot_.sendMessage(chat_id, "Failed to create the trip. Please try again.");
    }
}

void TripsConversation::handleDeleteConfirm(const bot::Update& update) {
    const std::string& data = update.callback_query.data;
    if (data == "confirm_delete") {
        if (activeTrip_) {
            tripRepo_.deleteTrip(activeTrip_->trip_id);
            allTrips_ = tripRepo_.getAllTrips(chat_id, 0);
            tripRepo_.updateActiveTrip(chat_id, 0, allTrips_.front().trip_id);
            activeTrip_ = tripRepo_.getActiveTrip(chat_id, 0);
        }
    }
    currentState_ = State::ShowingTrips;
    sendTripList(true);
}

void TripsConversation::closeConversation() {
    closed_ = true;
    if (message_id_ != 0) {
        bot_.editMessage(chat_id, message_id_, "Trip selection closed.");
    }
}
#ifndef TRIPS_CONVERSATION_H
#define TRIPS_CONVERSATION_H

#include "../bot/Bot.h"
#include "../repository/TripRepository.h"
#include <vector>
#include <optional>

class TripsConversation : public bot::Conversation {
public:
    TripsConversation(long long chat_id, long long user_id, bot::Bot& bot, TripRepository& tripRepo);
    ~TripsConversation() override;

    void handleUpdate(const bot::Update& update) override;
    bool isClosed() const override;

private:
    enum class State {
        ShowingTrips,
        CreatingTrip,
        ConfirmingDelete
    };

    void sendTripList(bool edit);
    void handleCallback(const bot::Update& update);
    void handleNewTripName(const bot::Update& update);
    void handleDeleteConfirm(const bot::Update& update);
    void closeConversation();

    bot::Bot& bot_;
    TripRepository& tripRepo_;

    State currentState_;
    std::vector<Trip> allTrips_;
    std::optional<Trip> activeTrip_;
    long long message_id_;
    bool closed_;
};

#endif // TRIPS_CONVERSATION_H
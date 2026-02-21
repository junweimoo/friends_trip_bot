#ifndef TRIP_REPOSITORY_H
#define TRIP_REPOSITORY_H

#include <string>
#include <vector>
#include <optional>

// Forward declaration
class DatabaseManager;

struct Trip {
    long long trip_id;
    long long chat_id;
    long long thread_id;
    std::string name;
    std::string gmt_created;
};

class TripRepository {
public:
    explicit TripRepository(DatabaseManager& dbManager);

    bool createDefaultChatAndTrip(long long chatId, long long threadId);

    long long createTrip(long long chatId, long long threadId, const std::string& name);
    
    std::optional<Trip> getTrip(long long tripId);

    std::vector<Trip> getAllTrips(long long chatId, long long threadId);
    
    bool updateTrip(const Trip& trip);
    
    bool deleteTrip(long long tripId);

    bool updateActiveTrip(long long chatId, long long threadId, long long tripId);

    std::optional<Trip> getActiveTrip(long long chatId, long long threadId);

private:
    DatabaseManager& dbManager_;
};

#endif // TRIP_REPOSITORY_H

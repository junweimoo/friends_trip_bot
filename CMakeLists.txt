cmake_minimum_required(VERSION 3.11)
project(friends_trip_bot)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# Find PostgreSQL (libpq)
find_package(PostgreSQL REQUIRED)

# Fetch libpqxx
FetchContent_Declare(
    libpqxx
    URL https://github.com/jtv/libpqxx/archive/refs/tags/7.10.5.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(SKIP_PQXX_STATIC ON CACHE BOOL "" FORCE)
set(SKIP_PQXX_DYNAMIC OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libpqxx)

#F
find_package(CURL REQUIRED)

add_executable(friends_trip_bot
    main.cpp
    bot/Bot.cpp
    bot/Conversation.cpp
    database/DatabaseManager.cpp
    database/DatabaseSchema.cpp
    repository/UserRepository.cpp
    repository/TripRepository.cpp
    repository/PaymentRepository.cpp
    service/UserService.cpp
    conversations/DemoConversation.cpp
    handlers/Handlers.cpp
)

target_link_libraries(friends_trip_bot
    PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pqxx
    PostgreSQL::PostgreSQL
)

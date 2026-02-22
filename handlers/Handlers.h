#ifndef HANDLERS_H
#define HANDLERS_H

#include "../bot/Bot.h"
#include "../service/UserService.h"

class PaymentRepository;
class TripRepository;

namespace handlers {

struct Services {
    UserService& userService;
};

struct Repositories {
    UserRepository& userRepository;
    TripRepository& tripRepository;
    PaymentRepository& paymentRepository;
};

void registerHandlers(bot::Bot& bot, const Services& services, const Repositories& repos);

} // namespace handlers

#endif // HANDLERS_H

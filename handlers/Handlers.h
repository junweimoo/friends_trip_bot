#ifndef HANDLERS_H
#define HANDLERS_H

#include "../bot/Bot.h"
#include "../service/UserService.h"

namespace handlers {

struct Services {
    UserService& userService;
};

void registerHandlers(bot::Bot& bot, const Services& services);

} // namespace handlers

#endif // HANDLERS_H

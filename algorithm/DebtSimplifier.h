#ifndef FRIENDS_TRIP_BOT_DEBTSIMPLIFIER_H
#define FRIENDS_TRIP_BOT_DEBTSIMPLIFIER_H

#include "../repository/PaymentRepository.h"
#include <unordered_map>
#include <vector>
#include <string>

struct SimplifiedPayment {
    long long from_user_id;
    long long to_user_id;
    double amount;
};

class DebtSimplifier {
public:
    static std::vector<SimplifiedPayment> simplifyDebts(
        const std::vector<PaymentGroup>& paymentGroups,
        const std::unordered_map<std::string, double>& exchangeRates);
};

#endif //FRIENDS_TRIP_BOT_DEBTSIMPLIFIER_H
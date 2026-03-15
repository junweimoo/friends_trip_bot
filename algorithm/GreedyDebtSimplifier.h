#ifndef FRIENDS_TRIP_BOT_GREEDYDEBTSIMPLIFIER_H
#define FRIENDS_TRIP_BOT_GREEDYDEBTSIMPLIFIER_H

#include "DebtSimplifier.h"

class GreedyDebtSimplifier : public DebtSimplifier {
protected:
    std::vector<SimplifiedPayment> solve(
        std::vector<std::pair<long long, long long>> creditors,
        std::vector<std::pair<long long, long long>> debtors,
        const std::string& targetCurrency) override;
};

#endif //FRIENDS_TRIP_BOT_GREEDYDEBTSIMPLIFIER_H

#ifndef FRIENDS_TRIP_BOT_MINTRANSACTIONSSIMPLIFIER_H
#define FRIENDS_TRIP_BOT_MINTRANSACTIONSSIMPLIFIER_H

#include "DebtSimplifier.h"

class MinTransactionsSimplifier : public DebtSimplifier {
protected:
    std::vector<SimplifiedPayment> solve(
        std::vector<std::pair<long long, long long>> creditors,
        std::vector<std::pair<long long, long long>> debtors,
        const std::string& targetCurrency) override;

private:
    // balances: (userId, signed balance) — positive=creditor, negative=debtor
    static void dfs(
        std::vector<std::pair<long long, long long>>& balances,
        int idx,
        std::vector<SimplifiedPayment>& current,
        const std::string& currency,
        int& minCount,
        std::vector<SimplifiedPayment>& best);
};

#endif //FRIENDS_TRIP_BOT_MINTRANSACTIONSSIMPLIFIER_H

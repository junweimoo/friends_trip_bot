#include "DebtSimplifier.h"
#include <algorithm>
#include <cmath>

std::vector<SimplifiedPayment> DebtSimplifier::simplifyDebts(
    const std::vector<PaymentGroup>& paymentGroups,
    const std::unordered_map<std::string, double>& exchangeRates) {

    // Compute net balance per user in target currency
    std::unordered_map<long long, double> balances;
    for (const auto& group : paymentGroups) {
        double rate = exchangeRates.at(group.currency);
        balances[group.payer_user_id] += group.total_amount * rate;
        for (const auto& record : group.records) {
            balances[record.to_user_id] -= record.amount * rate;
        }
    }

    // Separate into creditors and debtors
    std::vector<std::pair<long long, double>> creditors;
    std::vector<std::pair<long long, double>> debtors;

    for (const auto& [userId, balance] : balances) {
        if (balance > 0.01) {
            creditors.push_back({userId, balance});
        } else if (balance < -0.01) {
            debtors.push_back({userId, -balance});
        }
    }

    // Greedy matching: largest creditor with largest debtor
    std::vector<SimplifiedPayment> result;

    auto sortDesc = [](std::vector<std::pair<long long, double>>& v) {
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
    };

    sortDesc(creditors);
    sortDesc(debtors);

    while (!creditors.empty() && !debtors.empty()) {
        double settleAmount = std::min(creditors[0].second, debtors[0].second);
        settleAmount = std::round(settleAmount * 100.0) / 100.0;

        result.push_back({debtors[0].first, creditors[0].first, settleAmount});

        creditors[0].second -= settleAmount;
        debtors[0].second -= settleAmount;

        if (creditors[0].second < 0.01) {
            creditors.erase(creditors.begin());
        }
        if (debtors[0].second < 0.01) {
            debtors.erase(debtors.begin());
        }

        sortDesc(creditors);
        sortDesc(debtors);
    }

    return result;
}
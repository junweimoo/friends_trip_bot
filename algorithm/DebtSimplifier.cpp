#include "DebtSimplifier.h"
#include <algorithm>
#include <cmath>

std::vector<SimplifiedPayment> DebtSimplifier::simplifyDebts(
    const std::vector<PaymentGroup>& paymentGroups,
    const std::unordered_map<std::string, double>& exchangeRates,
    const std::string& targetCurrency) {
    double targetMinorPerMajor = MoneyAmount::minorUnitsPerMajor(targetCurrency);

    std::unordered_map<long long, double> balances;
    for (const auto& group : paymentGroups) {
        for (const auto& record : group.records) {
            double srcMinorPerMajor = MoneyAmount::minorUnitsPerMajor(record.amount.currency());
            double recConversionFactor = exchangeRates.at(record.amount.currency()) * targetMinorPerMajor / srcMinorPerMajor;

            double transactionAmountMinor = record.amount.minorAmount() * recConversionFactor;
            balances[record.to_user_id] -= transactionAmountMinor;
            balances[record.from_user_id] += transactionAmountMinor;
        }
    }

    std::vector<std::pair<long long, double>> creditors;
    std::vector<std::pair<long long, double>> debtors;

    for (const auto& [userId, balance] : balances) {
        if (balance > 0.5) {
            creditors.push_back({userId, balance});
        } else if (balance < -0.5) {
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
        creditors[0].second -= settleAmount;
        debtors[0].second -= settleAmount;

        long long settleMinor = static_cast<long long>(std::round(settleAmount));
        if (settleMinor > 0) {
            result.push_back({debtors[0].first, creditors[0].first,
                              MoneyAmount(targetCurrency, settleMinor)});
        }

        if (creditors[0].second < 0.5) {
            creditors.erase(creditors.begin());
        }
        if (!debtors.empty() && debtors[0].second < 0.5) {
            debtors.erase(debtors.begin());
        }

        sortDesc(creditors);
        sortDesc(debtors);
    }

    return result;
}
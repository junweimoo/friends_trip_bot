#include "DebtSimplifier.h"
#include <cmath>

std::pair<
    std::vector<std::pair<long long, long long>>,
    std::vector<std::pair<long long, long long>>
> DebtSimplifier::computeBalances(
    const std::vector<PaymentGroup>& paymentGroups,
    const std::unordered_map<std::string, double>& exchangeRates,
    const std::string& targetCurrency)
{
    double targetMinorPerMajor = MoneyAmount::minorUnitsPerMajor(targetCurrency);

    std::unordered_map<long long, double> balances;
    for (const auto& group : paymentGroups) {
        for (const auto& record : group.records) {
            double srcMinorPerMajor = MoneyAmount::minorUnitsPerMajor(record.amount.currency());
            double conversionFactor = exchangeRates.at(record.amount.currency()) * targetMinorPerMajor / srcMinorPerMajor;
            double amountMinor = record.amount.minorAmount() * conversionFactor;
            balances[record.to_user_id] -= amountMinor;
            balances[record.from_user_id] += amountMinor;
        }
    }

    std::vector<std::pair<long long, long long>> creditors;
    std::vector<std::pair<long long, long long>> debtors;
    for (const auto& [userId, balance] : balances) {
        long long rounded = static_cast<long long>(std::round(balance));
        if (rounded > 0) {
            creditors.push_back({userId, rounded});
        } else if (rounded < 0) {
            debtors.push_back({userId, -rounded});
        }
    }
    return {creditors, debtors};
}

std::vector<SimplifiedPayment> DebtSimplifier::simplifyDebts(
    const std::vector<PaymentGroup>& paymentGroups,
    const std::unordered_map<std::string, double>& exchangeRates,
    const std::string& targetCurrency)
{
    auto [creditors, debtors] = computeBalances(paymentGroups, exchangeRates, targetCurrency);
    return solve(std::move(creditors), std::move(debtors), targetCurrency);
}

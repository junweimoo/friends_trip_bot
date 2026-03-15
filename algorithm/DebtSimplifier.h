#ifndef FRIENDS_TRIP_BOT_DEBTSIMPLIFIER_H
#define FRIENDS_TRIP_BOT_DEBTSIMPLIFIER_H

#include "../repository/PaymentRepository.h"
#include "../utils/MoneyAmount.h"
#include <unordered_map>
#include <vector>
#include <string>

struct SimplifiedPayment {
    long long from_user_id;
    long long to_user_id;
    MoneyAmount amount;
};

class DebtSimplifier {
public:
    virtual ~DebtSimplifier() = default;

    std::vector<SimplifiedPayment> simplifyDebts(
        const std::vector<PaymentGroup>& paymentGroups,
        const std::unordered_map<std::string, double>& exchangeRates,
        const std::string& targetCurrency);

protected:
    virtual std::vector<SimplifiedPayment> solve(
        std::vector<std::pair<long long, long long>> creditors,
        std::vector<std::pair<long long, long long>> debtors,
        const std::string& targetCurrency) = 0;

private:
    static std::pair<
        std::vector<std::pair<long long, long long>>,
        std::vector<std::pair<long long, long long>>
    > computeBalances(
        const std::vector<PaymentGroup>& paymentGroups,
        const std::unordered_map<std::string, double>& exchangeRates,
        const std::string& targetCurrency);
};

#endif //FRIENDS_TRIP_BOT_DEBTSIMPLIFIER_H

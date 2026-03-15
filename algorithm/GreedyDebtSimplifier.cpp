#include "GreedyDebtSimplifier.h"
#include <queue>
#include <algorithm>

std::vector<SimplifiedPayment> GreedyDebtSimplifier::solve(
    std::vector<std::pair<long long, long long>> creditors,
    std::vector<std::pair<long long, long long>> debtors,
    const std::string& targetCurrency)
{
    // Max-heap: (balance, userId)
    using Entry = std::pair<long long, long long>;
    std::priority_queue<Entry> creditorHeap;
    for (const auto& [id, bal] : creditors) creditorHeap.push({bal, id});
    std::priority_queue<Entry> debtorHeap;
    for (const auto& [id, bal] : debtors) debtorHeap.push({bal, id});

    std::vector<SimplifiedPayment> result;
    while (!creditorHeap.empty() && !debtorHeap.empty()) {
        auto [credBal, credId] = creditorHeap.top(); creditorHeap.pop();
        auto [debtBal, debtId] = debtorHeap.top(); debtorHeap.pop();

        long long settleAmount = std::min(credBal, debtBal);
        result.push_back({debtId, credId, MoneyAmount(targetCurrency, settleAmount)});

        long long credRem = credBal - settleAmount;
        long long debtRem = debtBal - settleAmount;
        if (credRem > 0) creditorHeap.push({credRem, credId});
        if (debtRem > 0) debtorHeap.push({debtRem, debtId});
    }

    return result;
}

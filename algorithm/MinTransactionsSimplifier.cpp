#include "MinTransactionsSimplifier.h"
#include <algorithm>
#include <climits>

void MinTransactionsSimplifier::dfs(
    std::vector<std::pair<long long, long long>>& balances,
    int idx,
    std::vector<SimplifiedPayment>& current,
    const std::string& currency,
    int& minCount,
    std::vector<SimplifiedPayment>& best)
{
    // Advance past zeros
    while (idx < (int)balances.size() && balances[idx].second == 0) ++idx;
    if (idx == (int)balances.size()) {
        if ((int)current.size() < minCount) {
            minCount = (int)current.size();
            best = current;
        }
        return;
    }

    // Prune: can't improve on best found so far
    if ((int)current.size() >= minCount) return;

    long long b_i = balances[idx].second;
    for (int j = idx + 1; j < (int)balances.size(); ++j) {
        long long b_j = balances[j].second;
        if (b_j == 0) continue;
        if ((b_i > 0) == (b_j > 0)) continue; // same sign, skip

        long long amount = std::min(std::abs(b_i), std::abs(b_j));

        long long from_id, to_id;
        if (b_i > 0) { // idx is creditor, j is debtor
            from_id = balances[j].first;
            to_id = balances[idx].first;
        } else { // idx is debtor, j is creditor
            from_id = balances[idx].first;
            to_id = balances[j].first;
        }

        current.push_back({from_id, to_id, MoneyAmount(currency, amount)});
        balances[idx].second -= (b_i > 0 ? amount : -amount);
        balances[j].second -= (b_j > 0 ? amount : -amount);

        dfs(balances, idx, current, currency, minCount, best);

        balances[idx].second = b_i;
        balances[j].second = b_j;
        current.pop_back();
    }
}

std::vector<SimplifiedPayment> MinTransactionsSimplifier::solve(
    std::vector<std::pair<long long, long long>> creditors,
    std::vector<std::pair<long long, long long>> debtors,
    const std::string& targetCurrency)
{
    std::vector<std::pair<long long, long long>> balances;
    for (const auto& [id, bal] : creditors) balances.push_back({id, bal});
    for (const auto& [id, bal] : debtors) balances.push_back({id, -bal});

    if (balances.empty()) return {};

    std::vector<SimplifiedPayment> current, best;
    int minCount = INT_MAX;
    dfs(balances, 0, current, targetCurrency, minCount, best);
    return best;
}

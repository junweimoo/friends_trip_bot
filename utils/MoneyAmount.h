#ifndef FRIENDS_TRIP_BOT_MONEYAMOUNT_H
#define FRIENDS_TRIP_BOT_MONEYAMOUNT_H

#include <string>
#include <unordered_set>
#include <cmath>
#include <sstream>
#include <iomanip>

// Currencies that have no subunit (1 unit = 1 minor unit, no decimal places)
// e.g. JPY: 1 yen = 1 minor unit, vs USD: 1 dollar = 100 cents (minor units)
class MoneyAmount {
public:
    MoneyAmount() : currency_(""), amount_minor_(0) {}
    MoneyAmount(std::string currency, long long minorAmount)
        : currency_(std::move(currency)), amount_minor_(minorAmount) {}

    // Construct from a user-entered major-unit value (e.g. 12.34 for USD, 1234 for JPY)
    static MoneyAmount fromMajorUnits(const std::string& currency, double amount) {
        long long minor;
        if (isZeroDecimal(currency)) {
            minor = static_cast<long long>(std::round(amount));
        } else {
            minor = static_cast<long long>(std::round(amount * 100.0));
        }
        return MoneyAmount(currency, minor);
    }

    // Returns the factor for converting major units to minor units
    static double minorUnitsPerMajor(const std::string& currency) {
        return isZeroDecimal(currency) ? 1.0 : 100.0;
    }

    // Arithmetic operators — both operands must share the same currency
    MoneyAmount operator+(const MoneyAmount& other) const {
        return MoneyAmount(currency_, amount_minor_ + other.amount_minor_);
    }
    MoneyAmount operator-(const MoneyAmount& other) const {
        return MoneyAmount(currency_, amount_minor_ - other.amount_minor_);
    }
    // Scale by a floating-point factor (e.g. exchange rate)
    MoneyAmount operator*(double factor) const {
        return MoneyAmount(currency_, static_cast<long long>(std::round(amount_minor_ * factor)));
    }
    // Integer division (e.g. equal split among N people)
    MoneyAmount operator/(long long divisor) const {
        return MoneyAmount(currency_, amount_minor_ / divisor);
    }

    long long minorAmount() const { return amount_minor_; }
    const std::string& currency() const { return currency_; }

    // Format as a human-readable string, e.g. "12.34 USD" or "1234 JPY"
    std::string toHumanReadable() const {
        std::ostringstream ss;
        if (isZeroDecimal(currency_)) {
            ss << amount_minor_ << " " << currency_;
        } else {
            double major = static_cast<double>(amount_minor_) / 100.0;
            ss << std::fixed << std::setprecision(2) << major << " " << currency_;
        }
        return ss.str();
    }

    static bool isZeroDecimal(const std::string& currency) {
        static const std::unordered_set<std::string> zeroDecimalCurrencies = {
            "JPY", "KRW", "VND", "IDR", "ISK", "CLP",
            "UGX", "RWF", "BIF", "GNF", "XAF", "XOF", "XPF"
        };
        return zeroDecimalCurrencies.count(currency) > 0;
    }

private:
    std::string currency_;
    long long amount_minor_;
};

#endif // FRIENDS_TRIP_BOT_MONEYAMOUNT_H
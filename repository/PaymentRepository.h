#ifndef PAYMENT_REPOSITORY_H
#define PAYMENT_REPOSITORY_H

#include <string>
#include <vector>
#include <optional>

class DatabaseManager;

struct PaymentRecord {
    long long payment_record_id;
    long long payment_group_id;
    long long trip_id;
    double amount;
    std::string currency;
    long long from_user_id;
    long long to_user_id;
    std::string gmt_created;
};

struct PaymentGroup {
    long long payment_group_id;
    long long trip_id;
    std::string name;
    double total_amount;
    std::string currency;
    long long payer_user_id;
    std::string gmt_created;
    std::vector<PaymentRecord> records;
};

class PaymentRepository {
public:
    explicit PaymentRepository(DatabaseManager& dbManager);

    bool createPaymentGroup(const PaymentGroup& group);

    int getPaymentRecordCount(long long tripId);

    std::vector<PaymentGroup> getPaymentGroups(long long tripId, int pageSize, int pageNumber);

    std::vector<PaymentGroup> getAllPaymentGroups(long long tripId);

    std::vector<PaymentRecord> getAllPaymentRecords(long long tripId);

    bool deletePaymentGroup(long long paymentGroupId);

private:
    DatabaseManager& dbManager_;
};

#endif // PAYMENT_REPOSITORY_H
#ifndef PAYMENT_REPOSITORY_H
#define PAYMENT_REPOSITORY_H

#include <string>
#include <vector>
#include <optional>

class DatabaseManager;

struct PaymentRecord {
    long long payment_record_id;
    long long payment_group_id;
    long long user_id;
    double amount;
};

struct PaymentGroup {
    long long payment_group_id;
    long long trip_id;
    std::string name;
    std::string gmt_created;
    std::vector<PaymentRecord> records;
};

class PaymentRepository {
public:
    explicit PaymentRepository(DatabaseManager& dbManager);

    bool createPaymentGroup(long long tripId, const std::string& groupName, const std::vector<PaymentRecord>& records);

    int getPaymentRecordCount(long long tripId);

    std::vector<PaymentGroup> getPaymentGroups(long long tripId, int pageSize, int pageNumber);

    std::vector<PaymentGroup> getAllPaymentGroups(long long tripId);

    std::vector<PaymentRecord> getAllPaymentRecords(long long tripId);

    bool deletePaymentGroup(long long paymentGroupId);

private:
    DatabaseManager& dbManager_;
};

#endif // PAYMENT_REPOSITORY_H
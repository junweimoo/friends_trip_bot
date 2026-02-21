#include "PaymentRepository.h"
#include "../database/DatabaseManager.h"
#include <iostream>
#include <pqxx/pqxx>
#include <map>

PaymentRepository::PaymentRepository(DatabaseManager& dbManager) : dbManager_(dbManager) {}

bool PaymentRepository::createPaymentGroup(long long tripId, const std::string& groupName, const std::vector<PaymentRecord>& records) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);

        // 1. Create the Payment Group
        pqxx::result groupRes = txn.exec_params(
            "INSERT INTO payment_groups (trip_id, name) VALUES ($1, $2) RETURNING payment_group_id",
            tripId, groupName
        );

        if (groupRes.empty()) return false;
        long long groupId = groupRes[0][0].as<long long>();

        // 2. Create the Payment Records linked to the new group
        for (const auto& rec : records) {
            txn.exec_params(
                "INSERT INTO payment_records (payment_group_id, user_id, amount) VALUES ($1, $2, $3)",
                groupId, rec.user_id, rec.amount
            );
        }

        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating payment group: " << e.what() << std::endl;
        return false;
    }
}

int PaymentRepository::getPaymentRecordCount(long long tripId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return 0;

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_params(
            "SELECT COUNT(*) FROM payment_records WHERE trip_id = $1",
            tripId
        );

        if (res.empty()) return 0;
        return res[0][0].as<int>();
    } catch (const std::exception& e) {
        std::cerr << "Error getting payment record count: " << e.what() << std::endl;
    }

    return 0;
}

std::vector<PaymentGroup> PaymentRepository::getPaymentGroups(long long tripId, int pageSize, int pageNumber) {
    std::vector<PaymentGroup> groups;
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return groups;

    if (pageSize <= 0) return groups;
    if (pageNumber < 1) pageNumber = 1;

    long long offset = (static_cast<long long>(pageNumber) - 1) * pageSize;

    try {
        pqxx::work txn(*conn);

        pqxx::result res = txn.exec_params(
            "SELECT payment_group_id, trip_id, name, gmt_created FROM payment_groups "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC "
            "LIMIT $2 OFFSET $3",
            tripId, pageSize, offset
        );

        std::vector<long long> groupIds;
        for (const auto& row : res) {
            long long gId = row["payment_group_id"].as<long long>();
            groupIds.push_back(gId);
            groups.emplace_back(PaymentGroup{
                gId,
                row["trip_id"].as<long long>(),
                row["name"].c_str(),
                row["gmt_created"].c_str(),
                {}
            });
        }

        if (!groupIds.empty()) {
            return groups;
        }

        std::string idArray = "{";
        for (size_t i = 0; i < groupIds.size(); ++i) {
            if (i > 0) idArray += ",";
            idArray += std::to_string(groupIds[i]);
        }
        idArray += "}";

        pqxx::result recordRes = txn.exec_params(
            "SELECT payment_record_id, payment_group_id, user_id, amount "
            "FROM payment_records WHERE payment_group_id = ANY($1::bigint[]) "
            "ORDER BY gmt_created DESC",
            idArray
        );

        std::map<long long, PaymentGroup*> groupMap;
        for (auto& group : groups) {
            groupMap[group.payment_group_id] = &group;
        }

        for (const auto& row : recordRes) {
            long long gId = row["payment_group_id"].as<long long>();
            auto it = groupMap.find(gId);
            if (it != groupMap.end()) {
                it->second->records.emplace_back(PaymentRecord{
                    row["payment_record_id"].as<long long>(),
                    gId,
                    row["user_id"].as<long long>(),
                    row["amount"].as<double>()
                });
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting payment groups: " << e.what() << std::endl;
    }
    return groups;
}

std::vector<PaymentGroup> PaymentRepository::getAllPaymentGroups(long long tripId) {
    std::vector<PaymentGroup> groups;
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return groups;

    try {
        pqxx::work txn(*conn);

        pqxx::result res = txn.exec_params(
            "SELECT payment_group_id, trip_id, name, gmt_created FROM payment_groups "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            tripId
        );

        for (const auto& row : res) {
            groups.emplace_back(PaymentGroup{
                row["payment_group_id"].as<long long>(),
                row["trip_id"].as<long long>(),
                row["name"].c_str(),
                row["gmt_created"].c_str(),
                {}
            });
        }

        if (!groups.empty()) {
            return groups;
        }

        pqxx::result recordRes = txn.exec_params(
            "SELECT payment_record_id, payment_group_id, user_id, amount "
            "FROM payment_records WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            tripId
        );

        std::map<long long, PaymentGroup*> groupMap;
        for (auto& group : groups) {
            groupMap[group.payment_group_id] = &group;
        }

        for (const auto& row : recordRes) {
            long long gId = row["payment_group_id"].as<long long>();
            auto it = groupMap.find(gId);
            if (it != groupMap.end()) {
                it->second->records.emplace_back(PaymentRecord{
                    row["payment_record_id"].as<long long>(),
                    gId,
                    row["user_id"].as<long long>(),
                    row["amount"].as<double>()
                });
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting payment groups: " << e.what() << std::endl;
    }
    return groups;
}

std::vector<PaymentRecord> PaymentRepository::getAllPaymentRecords(long long tripId) {
    std::vector<PaymentRecord> records;
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return records;

    try {
        pqxx::work txn(*conn);

        pqxx::result res = txn.exec_params(
            "SELECT payment_record_id, payment_group_id, user_id, amount "
            "FROM payment_records "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            tripId
        );

        for (const auto& row : res) {
            records.emplace_back(PaymentRecord{
                row["payment_record_id"].as<long long>(),
                row["payment_group_id"].as<long long>(),
                row["user_id"].as<long long>(),
                row["amount"].as<double>()
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting all payment records: " << e.what() << std::endl;
    }
    return records;
}

bool PaymentRepository::deletePaymentGroup(long long paymentGroupId) {
    std::vector<PaymentRecord> records;
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_params(
            "DELETE FROM payment_groups WHERE group_id = $1",
            paymentGroupId
        );
        txn.commit();
        return res.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting payment record: " << e.what() << std::endl;
    }

    return true;
}
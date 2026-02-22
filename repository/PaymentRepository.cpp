#include "PaymentRepository.h"
#include "../database/DatabaseManager.h"
#include <iostream>
#include <pqxx/pqxx>
#include <map>

PaymentRepository::PaymentRepository(DatabaseManager& dbManager) : dbManager_(dbManager) {}

bool PaymentRepository::createPaymentGroup(const PaymentGroup& group) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);

        // 1. Create the Payment Group
        pqxx::result groupRes = txn.exec_params(
            "INSERT INTO payment_groups (trip_id, name, total_amount, currency, payer_user_id) VALUES ($1, $2, $3, $4, $5) RETURNING group_id",
            group.trip_id, group.name, group.total_amount, group.currency, group.payer_user_id
        );

        if (groupRes.empty()) return false;
        long long groupId = groupRes[0][0].as<long long>();

        // 2. Create the Payment Records linked to the new group
        for (const auto& rec : group.records) {
            txn.exec_params(
                "INSERT INTO payment_records (group_id, trip_id, amount, currency, from_user_id, to_user_id) VALUES ($1, $2, $3, $4, $5, $6)",
                groupId, group.trip_id, rec.amount, rec.currency, rec.from_user_id, rec.to_user_id
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
            "SELECT group_id, trip_id, name, total_amount, currency, payer_user_id, gmt_created FROM payment_groups "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC "
            "LIMIT $2 OFFSET $3",
            tripId, pageSize, offset
        );

        std::vector<long long> groupIds;
        for (const auto& row : res) {
            long long gId = row["group_id"].as<long long>();
            groupIds.push_back(gId);
            groups.emplace_back(PaymentGroup{
                gId,
                row["trip_id"].as<long long>(),
                row["name"].c_str(),
                row["total_amount"].as<double>(),
                row["currency"].c_str(),
                row["payer_user_id"].as<long long>(),
                row["gmt_created"].c_str(),
                {}
            });
        }

        if (groupIds.empty()) {
            return groups;
        }

        std::string idArray = "{";
        for (size_t i = 0; i < groupIds.size(); ++i) {
            if (i > 0) idArray += ",";
            idArray += std::to_string(groupIds[i]);
        }
        idArray += "}";

        pqxx::result recordRes = txn.exec_params(
            "SELECT record_id, group_id, trip_id, amount, currency, from_user_id, to_user_id, gmt_created "
            "FROM payment_records WHERE group_id = ANY($1::bigint[]) "
            "ORDER BY gmt_created DESC",
            idArray
        );

        std::map<long long, PaymentGroup*> groupMap;
        for (auto& group : groups) {
            groupMap[group.payment_group_id] = &group;
        }

        for (const auto& row : recordRes) {
            long long gId = row["group_id"].as<long long>();
            auto it = groupMap.find(gId);
            if (it != groupMap.end()) {
                it->second->records.emplace_back(PaymentRecord{
                    row["record_id"].as<long long>(),
                    gId,
                    row["trip_id"].as<long long>(),
                    row["amount"].as<double>(),
                    row["currency"].c_str(),
                    row["from_user_id"].as<long long>(),
                    row["to_user_id"].as<long long>(),
                    row["gmt_created"].c_str()
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
            "SELECT group_id, trip_id, name, total_amount, currency, payer_user_id, gmt_created FROM payment_groups "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            tripId
        );

        std::vector<long long> groupIds;
        for (const auto& row : res) {
            long long gId = row["group_id"].as<long long>();
            groupIds.push_back(gId);
            groups.emplace_back(PaymentGroup{
                gId,
                row["trip_id"].as<long long>(),
                row["name"].c_str(),
                row["total_amount"].as<double>(),
                row["currency"].c_str(),
                row["payer_user_id"].as<long long>(),
                row["gmt_created"].c_str(),
                {}
            });
        }

        if (groups.empty()) {
            return groups;
        }

        pqxx::result recordRes = txn.exec_params(
            "SELECT record_id, group_id, trip_id, amount, currency, from_user_id, to_user_id, gmt_created "
            "FROM payment_records WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            tripId
        );

        std::map<long long, PaymentGroup*> groupMap;
        for (auto& group : groups) {
            groupMap[group.payment_group_id] = &group;
        }

        for (const auto& row : recordRes) {
            long long gId = row["group_id"].as<long long>();
            auto it = groupMap.find(gId);
            if (it != groupMap.end()) {
                it->second->records.emplace_back(PaymentRecord{
                    row["record_id"].as<long long>(),
                    gId,
                    row["trip_id"].as<long long>(),
                    row["amount"].as<double>(),
                    row["currency"].c_str(),
                    row["from_user_id"].as<long long>(),
                    row["to_user_id"].as<long long>(),
                    row["gmt_created"].c_str()
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
            "SELECT record_id, group_id, trip_id, amount, currency, from_user_id, to_user_id, gmt_created "
            "FROM payment_records "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            tripId
        );

        for (const auto& row : res) {
            records.emplace_back(PaymentRecord{
                row["record_id"].as<long long>(),
                row["group_id"].as<long long>(),
                row["trip_id"].as<long long>(),
                row["amount"].as<double>(),
                row["currency"].c_str(),
                row["from_user_id"].as<long long>(),
                row["to_user_id"].as<long long>(),
                row["gmt_created"].c_str()
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting all payment records: " << e.what() << std::endl;
    }
    return records;
}

bool PaymentRepository::deletePaymentGroup(long long paymentGroupId) {
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
        std::cerr << "Error deleting payment group: " << e.what() << std::endl;
    }

    return false;
}

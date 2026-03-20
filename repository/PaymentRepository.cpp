#include "PaymentRepository.h"
#include "../database/DatabaseManager.h"
#include "../utils/utils.h"
#include <iostream>
#include <pqxx/pqxx>
#include <map>
#include <chrono>

PaymentRepository::PaymentRepository(DatabaseManager& dbManager) : dbManager_(dbManager) {}

bool PaymentRepository::createPaymentGroup(const PaymentGroup& group) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error creating payment group: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);

        // 1. Create the Payment Group
        pqxx::result groupRes = txn.exec(
            "INSERT INTO payment_groups (trip_id, name, total_amount, currency, payer_user_id) VALUES ($1, $2, $3, $4, $5) RETURNING group_id",
            pqxx::params{group.trip_id, group.name, group.total_amount.minorAmount(), group.total_amount.currency(), group.payer_user_id}
        );

        if (groupRes.empty()) return false;
        long long groupId = groupRes[0][0].as<long long>();

        // 2. Create the Payment Records linked to the new group
        for (const auto& rec : group.records) {
            txn.exec(
                "INSERT INTO payment_records (group_id, trip_id, amount, currency, from_user_id, to_user_id) VALUES ($1, $2, $3, $4, $5, $6)",
                pqxx::params{groupId, group.trip_id, rec.amount.minorAmount(), rec.amount.currency(), rec.from_user_id, rec.to_user_id}
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
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting payment record count: database connection unavailable" << std::endl;
        return 0;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "SELECT COUNT(*) FROM payment_records WHERE trip_id = $1",
            pqxx::params{tripId}
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
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting payment groups: database connection unavailable" << std::endl;
        return groups;
    }

    if (pageSize <= 0) return groups;
    if (pageNumber < 1) pageNumber = 1;

    long long offset = (static_cast<long long>(pageNumber) - 1) * pageSize;

    try {
        pqxx::work txn(*conn);

        pqxx::result res = txn.exec(
            "SELECT group_id, trip_id, name, total_amount, currency, payer_user_id, gmt_created FROM payment_groups "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC "
            "LIMIT $2 OFFSET $3",
            pqxx::params{tripId, pageSize, offset}
        );

        std::vector<long long> groupIds;
        for (const auto& row : res) {
            long long gId = row["group_id"].as<long long>();
            groupIds.push_back(gId);
            groups.emplace_back(PaymentGroup{
                gId,
                row["trip_id"].as<long long>(),
                row["name"].c_str(),
                MoneyAmount(row["currency"].c_str(), row["total_amount"].as<long long>()),
                row["payer_user_id"].as<long long>(),
                utils::parseTimestamp(row["gmt_created"].c_str()),
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

        pqxx::result recordRes = txn.exec(
            "SELECT record_id, group_id, trip_id, amount, currency, from_user_id, to_user_id, gmt_created "
            "FROM payment_records WHERE group_id = ANY($1::bigint[]) "
            "ORDER BY gmt_created DESC",
            pqxx::params{idArray}
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
                    MoneyAmount(row["currency"].c_str(), row["amount"].as<long long>()),
                    row["from_user_id"].as<long long>(),
                    row["to_user_id"].as<long long>(),
                    utils::parseTimestamp(row["gmt_created"].c_str())
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
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting all payment groups: database connection unavailable" << std::endl;
        return groups;
    }

    try {
        pqxx::work txn(*conn);

        pqxx::result res = txn.exec(
            "SELECT group_id, trip_id, name, total_amount, currency, payer_user_id, gmt_created FROM payment_groups "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            pqxx::params{tripId}
        );

        std::vector<long long> groupIds;
        for (const auto& row : res) {
            long long gId = row["group_id"].as<long long>();
            groupIds.push_back(gId);
            groups.emplace_back(PaymentGroup{
                gId,
                row["trip_id"].as<long long>(),
                row["name"].c_str(),
                MoneyAmount(row["currency"].c_str(), row["total_amount"].as<long long>()),
                row["payer_user_id"].as<long long>(),
                utils::parseTimestamp(row["gmt_created"].c_str()),
                {}
            });
        }

        if (groups.empty()) {
            return groups;
        }

        pqxx::result recordRes = txn.exec(
            "SELECT record_id, group_id, trip_id, amount, currency, from_user_id, to_user_id, gmt_created "
            "FROM payment_records WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            pqxx::params{tripId}
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
                    MoneyAmount(row["currency"].c_str(), row["amount"].as<long long>()),
                    row["from_user_id"].as<long long>(),
                    row["to_user_id"].as<long long>(),
                    utils::parseTimestamp(row["gmt_created"].c_str())
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
    if (!conn || !conn->is_open()) {
        std::cerr << "Error getting all payment records: database connection unavailable" << std::endl;
        return records;
    }

    try {
        pqxx::work txn(*conn);

        pqxx::result res = txn.exec(
            "SELECT record_id, group_id, trip_id, amount, currency, from_user_id, to_user_id, gmt_created "
            "FROM payment_records "
            "WHERE trip_id = $1 "
            "ORDER BY gmt_created DESC",
            pqxx::params{tripId}
        );

        for (const auto& row : res) {
            records.emplace_back(PaymentRecord{
                row["record_id"].as<long long>(),
                row["group_id"].as<long long>(),
                row["trip_id"].as<long long>(),
                MoneyAmount(row["currency"].c_str(), row["amount"].as<long long>()),
                row["from_user_id"].as<long long>(),
                row["to_user_id"].as<long long>(),
                utils::parseTimestamp(row["gmt_created"].c_str())
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting all payment records: " << e.what() << std::endl;
    }
    return records;
}

std::optional<PaymentGroup> PaymentRepository::deleteLastPaymentGroup(long long tripId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error deleting last payment group: database connection unavailable" << std::endl;
        return std::nullopt;
    }

    try {
        pqxx::work txn(*conn);

        pqxx::result groupRes = txn.exec(
            "SELECT group_id, trip_id, name, total_amount, currency, payer_user_id, gmt_created "
            "FROM payment_groups WHERE trip_id = $1 ORDER BY gmt_created DESC LIMIT 1",
            pqxx::params{tripId}
        );

        if (groupRes.empty()) return std::nullopt;

        const auto& groupRow = groupRes[0];
        long long groupId = groupRow["group_id"].as<long long>();

        PaymentGroup group{
            groupId,
            groupRow["trip_id"].as<long long>(),
            groupRow["name"].c_str(),
            MoneyAmount(groupRow["currency"].c_str(), groupRow["total_amount"].as<long long>()),
            groupRow["payer_user_id"].as<long long>(),
            utils::parseTimestamp(groupRow["gmt_created"].c_str()),
            {}
        };

        pqxx::result recordRes = txn.exec(
            "SELECT record_id, group_id, trip_id, amount, currency, from_user_id, to_user_id, gmt_created "
            "FROM payment_records WHERE group_id = $1 ORDER BY gmt_created DESC",
            pqxx::params{groupId}
        );

        for (const auto& row : recordRes) {
            group.records.emplace_back(PaymentRecord{
                row["record_id"].as<long long>(),
                groupId,
                row["trip_id"].as<long long>(),
                MoneyAmount(row["currency"].c_str(), row["amount"].as<long long>()),
                row["from_user_id"].as<long long>(),
                row["to_user_id"].as<long long>(),
                utils::parseTimestamp(row["gmt_created"].c_str())
            });
        }

        txn.exec("DELETE FROM payment_records WHERE group_id = $1", pqxx::params{groupId});
        txn.exec("DELETE FROM payment_groups WHERE group_id = $1", pqxx::params{groupId});

        txn.commit();
        return group;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting last payment group: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool PaymentRepository::deletePaymentGroup(long long paymentGroupId) {
    pqxx::connection* conn = dbManager_.getConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "Error deleting payment group: database connection unavailable" << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(
            "DELETE FROM payment_groups WHERE group_id = $1",
            pqxx::params{paymentGroupId}
        );
        txn.commit();
        return res.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting payment group: " << e.what() << std::endl;
    }

    return false;
}

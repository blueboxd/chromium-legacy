// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leaked_credentials_table.h"

#include "components/password_manager/core/browser/sql_table_builder.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace password_manager {
namespace {

constexpr char kLeakedCredentialsTableName[] = "leaked_credentials";

// Represents columns of the leaked credentials table. Used with SQL
// queries that use all the columns.
enum class LeakedCredentialsTableColumn {
  kUrl,
  kUsername,
  kCreateTime,
  kMaxValue = kCreateTime
};

// Casts the leaked credentials table column enum to its integer value.
int GetColumnNumber(LeakedCredentialsTableColumn column) {
  return static_cast<int>(column);
}

// Teaches |builder| about the different DB schemes in different versions.
void InitializeLeakedCredentialsBuilder(SQLTableBuilder* builder) {
  // Version 0.
  builder->AddColumnToUniqueKey("url", "VARCHAR NOT NULL");
  builder->AddColumnToUniqueKey("username", "VARCHAR NOT NULL");
  builder->AddColumn("create_time", "INTEGER NOT NULL");
  builder->AddIndex("leaked_credentials_index", {"url", "username"});
  builder->SealVersion();
}

// Returns a leaked credentials vector from the SQL statement.
std::vector<LeakedCredentials> StatementToLeakedCredentials(sql::Statement* s) {
  std::vector<LeakedCredentials> results;
  while (s->Step()) {
    GURL url(
        s->ColumnString(GetColumnNumber(LeakedCredentialsTableColumn::kUrl)));
    base::string16 username = s->ColumnString16(
        GetColumnNumber(LeakedCredentialsTableColumn::kUsername));
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        (base::TimeDelta::FromMicroseconds(s->ColumnInt64(
            GetColumnNumber(LeakedCredentialsTableColumn::kCreateTime)))));
    results.push_back(
        LeakedCredentials(std::move(url), std::move(username), create_time));
  }
  return results;
}

}  // namespace

bool operator==(const LeakedCredentials& lhs, const LeakedCredentials& rhs) {
  return lhs.url == rhs.url && lhs.username == rhs.username &&
         lhs.create_time == rhs.create_time;
}

void LeakedCredentialsTable::Init(sql::Database* db) {
  db_ = db;
}

bool LeakedCredentialsTable::CreateTableIfNecessary() {
  if (!db_->DoesTableExist(kLeakedCredentialsTableName)) {
    SQLTableBuilder builder(kLeakedCredentialsTableName);
    InitializeLeakedCredentialsBuilder(&builder);
    if (!builder.CreateTable(db_))
      return false;
  }
  return true;
}

bool LeakedCredentialsTable::AddRow(
    const LeakedCredentials& leaked_credentials) {
  if (!leaked_credentials.url.is_valid())
    return false;
  sql::Statement s(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "INSERT OR IGNORE INTO leaked_credentials "
                              "(url, username, create_time) "
                              "VALUES (?, ?, ?)"));
  s.BindString(GetColumnNumber(LeakedCredentialsTableColumn::kUrl),
               leaked_credentials.url.spec());
  s.BindString16(GetColumnNumber(LeakedCredentialsTableColumn::kUsername),
                 leaked_credentials.username);
  s.BindInt64(GetColumnNumber(LeakedCredentialsTableColumn::kCreateTime),
              leaked_credentials.create_time.ToDeltaSinceWindowsEpoch()
                  .InMicroseconds());
  return s.Run();
}

bool LeakedCredentialsTable::RemoveRow(const GURL& url,
                                       const base::string16& username) {
  if (!url.is_valid())
    return false;
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM leaked_credentials WHERE url = ? AND username = ? "));
  s.BindString(0, url.spec());
  s.BindString16(1, username);
  return s.Run();
}

bool LeakedCredentialsTable::RemoveRowsByUrlAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  if (remove_end.is_null())
    remove_end = base::Time::Max();

  const int64_t remove_begin_us =
      remove_begin.ToDeltaSinceWindowsEpoch().InMicroseconds();
  const int64_t remove_end_us =
      remove_end.ToDeltaSinceWindowsEpoch().InMicroseconds();

  // If |url_filter| is null, remove all records in given date range.
  if (!url_filter) {
    sql::Statement s(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "DELETE FROM leaked_credentials WHERE "
                                "create_time >= ? AND create_time < ?"));
    s.BindInt64(0, remove_begin_us);
    s.BindInt64(1, remove_end_us);
    return s.Run();
  }

  // Otherwise, filter urls.
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT DISTINCT url FROM leaked_credentials WHERE "
      "create_time >= ? AND create_time < ?"));
  s.BindInt64(0, remove_begin_us);
  s.BindInt64(1, remove_end_us);

  std::vector<std::string> urls;
  while (s.Step()) {
    std::string url = s.ColumnString(0);
    if (url_filter.Run(GURL(url))) {
      urls.push_back(std::move(url));
    }
  }

  bool success = true;
  for (const std::string& url : urls) {
    sql::Statement s(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "DELETE FROM leaked_credentials WHERE url = ? "
                                "AND create_time >= ? AND create_time < ?"));
    s.BindString(0, url);
    s.BindInt64(1, remove_begin_us);
    s.BindInt64(2, remove_end_us);
    success = success && s.Run();
  }
  return success;
}

std::vector<LeakedCredentials> LeakedCredentialsTable::GetAllRows() {
  static constexpr char query[] =
      "SELECT url, username, create_time FROM leaked_credentials";
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE, query));
  return StatementToLeakedCredentials(&s);
}

}  // namespace password_manager

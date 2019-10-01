// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAKED_CREDENTIALS_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAKED_CREDENTIALS_TABLE_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace sql {
class Database;
}

namespace password_manager {

// Represents information about the particular credentials leak.
struct LeakedCredentials {
  LeakedCredentials(GURL url, base::string16 username, base::Time create_time)
      : url(std::move(url)),
        username(std::move(username)),
        create_time(create_time) {}

  // The url of the website of the leak.
  GURL url;
  // The value of the leaked username.
  base::string16 username;
  // The date when the record was created.
  base::Time create_time;
};

bool operator==(const LeakedCredentials& lhs, const LeakedCredentials& rhs);

// Represents the 'leaked credentials' table in the Login Database.
class LeakedCredentialsTable {
 public:
  LeakedCredentialsTable() = default;
  ~LeakedCredentialsTable() = default;

  // Initializes |db_|.
  void Init(sql::Database* db);

  // Creates the leaked credentials table if it doesn't exist.
  bool CreateTableIfNecessary();

  // Adds information about the leaked credentials. Returns true if the SQL
  // completed successfully.
  bool AddRow(const LeakedCredentials& leaked_credentials);

  // Removes information about the credentials leaked for |username| on |url|.
  // Returns true if the SQL completed successfully.
  bool RemoveRow(const GURL& url, const base::string16& username);

  // Returns all leaked credentials from the database.
  std::vector<LeakedCredentials> GetAllRows();

 private:
  sql::Database* db_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LeakedCredentialsTable);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAKED_CREDENTIALS_TABLE_H_

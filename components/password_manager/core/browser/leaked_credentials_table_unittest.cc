// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leaked_credentials_table.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

const char kTestDomain[] = "http://example.com";
const char kTestDomain2[] = "http://test.com";
const char kUsername[] = "user";
const char kUsername2[] = "user2";

using testing::ElementsAre;
using testing::IsEmpty;

class LeakedCredentialsTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ReloadDatabase();
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.GetPath().AppendASCII("TestDatabase");
    db_.reset(new LeakedCredentialsTable);
    connection_.reset(new sql::Database);
    connection_->set_exclusive_locking();
    ASSERT_TRUE(connection_->Open(file));
    db_->Init(connection_.get());
    ASSERT_TRUE(db_->CreateTableIfNecessary());
  }

  LeakedCredentials& test_data() { return test_data_; }
  LeakedCredentialsTable* db() { return db_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> connection_;
  std::unique_ptr<LeakedCredentialsTable> db_;
  LeakedCredentials test_data_{GURL(kTestDomain), base::ASCIIToUTF16(kUsername),
                               base::Time::FromTimeT(1)};
};

TEST_F(LeakedCredentialsTableTest, Sanity) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(test_data()));
  EXPECT_TRUE(db()->RemoveRow(test_data().url, test_data().username));
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
}

TEST_F(LeakedCredentialsTableTest, Reload) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  ReloadDatabase();
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(test_data()));
}

TEST_F(LeakedCredentialsTableTest, SameUrlDifferentUsername) {
  LeakedCredentials leaked_credentials1 = test_data();
  LeakedCredentials leaked_credentials2 = test_data();
  leaked_credentials2.username = base::ASCIIToUTF16(kUsername2);

  EXPECT_TRUE(db()->AddRow(leaked_credentials1));
  EXPECT_TRUE(db()->AddRow(leaked_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(leaked_credentials1, leaked_credentials2));
}

TEST_F(LeakedCredentialsTableTest, SameUsernameDifferentUrl) {
  LeakedCredentials leaked_credentials1 = test_data();
  LeakedCredentials leaked_credentials2 = test_data();
  leaked_credentials2.url = GURL(kTestDomain2);

  EXPECT_TRUE(db()->AddRow(leaked_credentials1));
  EXPECT_TRUE(db()->AddRow(leaked_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(leaked_credentials1, leaked_credentials2));
}

TEST_F(LeakedCredentialsTableTest, SameUrlAndUsernameDifferentTime) {
  LeakedCredentials leaked_credentials1 = test_data();
  LeakedCredentials leaked_credentials2 = test_data();
  leaked_credentials2.create_time = base::Time::FromTimeT(2);

  EXPECT_TRUE(db()->AddRow(leaked_credentials1));
  // It should return true as the sql statement ran correctly. It ignored
  // new row though because of unique constraints, hence there is only one
  // record in the database.
  EXPECT_TRUE(db()->AddRow(leaked_credentials2));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(leaked_credentials1));
}

TEST_F(LeakedCredentialsTableTest, BadURL) {
  test_data().url = GURL("bad");
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().url, test_data().username));
}

TEST_F(LeakedCredentialsTableTest, EmptyURL) {
  test_data().url = GURL();
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().url, test_data().username));
}

}  // namespace
}  // namespace password_manager

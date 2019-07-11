// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using ::autofill::PasswordForm;

TEST(CSVPassword, Construction) {
  // Default.
  CSVPassword empty;
  EXPECT_FALSE(empty.Parse(nullptr));

  // From CSV.
  CSVPassword::ColumnMap col_map = {
      {0, CSVPassword::Label::kOrigin},
      {1, CSVPassword::Label::kUsername},
      {2, CSVPassword::Label::kPassword},
  };
  CSVPassword csv_pwd(col_map, "http://example.com,user,password");
  PasswordForm result = csv_pwd.ParseValid();
  GURL expected_origin("http://example.com");
  EXPECT_EQ(expected_origin, result.origin);
  EXPECT_EQ(expected_origin.GetOrigin().spec(), result.signon_realm);
  EXPECT_EQ(base::ASCIIToUTF16("user"), result.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("password"), result.password_value);

  // Copy, move and operator=.
  CSVPassword copy = csv_pwd;
  EXPECT_EQ(copy.ParseValid(), csv_pwd.ParseValid());

  CSVPassword moved = std::move(copy);
  EXPECT_EQ(moved.ParseValid(), csv_pwd.ParseValid());

  CSVPassword target;
  target = csv_pwd;
  EXPECT_EQ(target.ParseValid(), csv_pwd.ParseValid());
  target = std::move(moved);
  EXPECT_EQ(target.ParseValid(), csv_pwd.ParseValid());
}

struct TestCase {
  std::string name;
  // Input.
  CSVPassword::ColumnMap map;
  std::string csv;
  // Expected.
  std::string origin;
  std::string signon_realm;
  std::string username;
  std::string password;
};

class TestCaseBuilder {
 public:
  TestCaseBuilder(std::string name) { test_case_.name = std::move(name); }

  ~TestCaseBuilder() = default;

  TestCaseBuilder& Map(CSVPassword::ColumnMap map) {
    test_case_.map = std::move(map);
    return *this;
  }

  TestCaseBuilder& CSV(std::string csv) {
    test_case_.csv = std::move(csv);
    return *this;
  }

  TestCaseBuilder& Origin(std::string origin) {
    test_case_.origin = std::move(origin);
    return *this;
  }

  TestCaseBuilder& SignonRealm(std::string signon_realm) {
    test_case_.signon_realm = std::move(signon_realm);
    return *this;
  }

  TestCaseBuilder& Username(std::string username) {
    test_case_.username = std::move(username);
    return *this;
  }

  TestCaseBuilder& Password(std::string password) {
    test_case_.password = std::move(password);
    return *this;
  }

  TestCase Build() { return std::move(test_case_); }

 private:
  TestCase test_case_;
};

class CSVPasswordTestSuccess : public ::testing::TestWithParam<TestCase> {};

TEST_P(CSVPasswordTestSuccess, Parse) {
  const TestCase& test_case = GetParam();
  SCOPED_TRACE(test_case.name);
  CSVPassword csv_pwd(test_case.map, test_case.csv);
  EXPECT_TRUE(csv_pwd.Parse(nullptr));

  PasswordForm result = csv_pwd.ParseValid();

  GURL expected_origin(test_case.origin);
  EXPECT_EQ(expected_origin, result.origin);
  EXPECT_EQ(expected_origin.GetOrigin().spec(), result.signon_realm);

  EXPECT_EQ(base::ASCIIToUTF16(test_case.username), result.username_value);
  EXPECT_EQ(base::ASCIIToUTF16(test_case.password), result.password_value);

  PasswordForm result2 = csv_pwd.ParseValid();
  EXPECT_EQ(result, result2);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CSVPasswordTestSuccess,
    ::testing::Values(TestCaseBuilder("all columns specified")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername},
                                {2, CSVPassword::Label::kPassword}})
                          .CSV("http://example.com,user,password")
                          .Origin("http://example.com")
                          .SignonRealm("http://example.com/")
                          .Username("user")
                          .Password("password")
                          .Build(),
                      TestCaseBuilder("empty username")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kPassword},
                                {2, CSVPassword::Label::kUsername}})
                          .CSV("http://example.com,password,")
                          .Origin("http://example.com")
                          .SignonRealm("http://example.com/")
                          .Username("")
                          .Password("password")
                          .Build(),
                      TestCaseBuilder("permuted and inserted columns")
                          .Map({{2, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername},
                                {4, CSVPassword::Label::kPassword}})
                          .CSV("X,the-user,https://example.org,Y,pwd")
                          .Origin("https://example.org")
                          .SignonRealm("https://example.org/")
                          .Username("the-user")
                          .Password("pwd")
                          .Build(),
                      TestCaseBuilder("Android")
                          .Map({{2, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername},
                                {0, CSVPassword::Label::kPassword}})
                          .CSV("pwd,the-user,android://example,Y,X")
                          .Origin("android://example")
                          .SignonRealm("android://example")
                          .Username("the-user")
                          .Password("pwd")
                          .Build(),
                      TestCaseBuilder("path discarded")
                          .Map({{2, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername},
                                {0, CSVPassword::Label::kPassword}})
                          .CSV("password,user,http://example.com/path")
                          .Origin("http://example.com/path")
                          .SignonRealm("http://example.com/")
                          .Username("user")
                          .Password("password")
                          .Build()));

class CSVPasswordTestFailure : public ::testing::TestWithParam<TestCase> {};

TEST_P(CSVPasswordTestFailure, Parse) {
  const TestCase& test_case = GetParam();
  SCOPED_TRACE(test_case.name);
  EXPECT_FALSE(CSVPassword(test_case.map, test_case.csv).Parse(nullptr));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CSVPasswordTestFailure,
    ::testing::Values(TestCaseBuilder("no columns specified")
                          .Map({})
                          .CSV("http://example.com,user,password")
                          .Build(),
                      TestCaseBuilder("not ASCII")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername},
                                {2, CSVPassword::Label::kPassword}})
                          .CSV("http://example.com/ř,user,password")
                          .Build(),
                      TestCaseBuilder("no origin in map")
                          .Map({{1, CSVPassword::Label::kUsername},
                                {2, CSVPassword::Label::kPassword}})
                          .CSV("http://example.com,user,password")
                          .Build(),
                      TestCaseBuilder("no username in map")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {2, CSVPassword::Label::kPassword}})
                          .CSV("http://example.com,user,password")
                          .Build(),
                      TestCaseBuilder("no password in map")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername}})
                          .CSV("http://example.com,user,password")
                          .Build(),
                      TestCaseBuilder("no origin in CSV")
                          .Map({{0, CSVPassword::Label::kUsername},
                                {1, CSVPassword::Label::kPassword},
                                {2, CSVPassword::Label::kOrigin}})
                          .CSV("user,password")
                          .Build(),
                      TestCaseBuilder("no username in CSV")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kPassword},
                                {2, CSVPassword::Label::kUsername}})
                          .CSV("http://example.com,password")
                          .Build(),
                      TestCaseBuilder("no password in CSV")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername},
                                {2, CSVPassword::Label::kPassword}})
                          .CSV("http://example.com,user")
                          .Build(),
                      TestCaseBuilder("malformed CSV")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername},
                                {2, CSVPassword::Label::kPassword}})
                          .CSV("\"")
                          .Build(),
                      TestCaseBuilder("map not injective")
                          .Map({{0, CSVPassword::Label::kOrigin},
                                {1, CSVPassword::Label::kUsername},
                                {2, CSVPassword::Label::kPassword},
                                {3, CSVPassword::Label::kUsername}})
                          .CSV("http://example.com,user,pwd,user2")
                          .Build()));

}  // namespace password_manager

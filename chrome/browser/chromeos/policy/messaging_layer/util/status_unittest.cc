// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/messaging_layer/util/status.h"

#include <stdio.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

TEST(Status, Empty) {
  Status status;
  EXPECT_EQ(error::OK, status.error_code());
  EXPECT_EQ(error::OK, status.code());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, GenericCodes) {
  EXPECT_EQ(error::OK, Status::StatusOK().error_code());
  EXPECT_EQ(error::OK, Status::StatusOK().code());
  EXPECT_EQ("OK", Status::StatusOK().ToString());
}

TEST(Status, OkConstructorIgnoresMessage) {
  Status status(error::OK, "msg");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, CheckOK) {
  Status status;
  CHECK_OK(status);
  CHECK_OK(status) << "Failed";
  DCHECK_OK(status) << "Failed";
}

TEST(Status, ErrorMessage) {
  Status status(error::INVALID_ARGUMENT, "");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ("", status.error_message());
  EXPECT_EQ("", status.message());
  EXPECT_EQ("INVALID_ARGUMENT", status.ToString());
  status = Status(error::INVALID_ARGUMENT, "msg");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ("msg", status.error_message());
  EXPECT_EQ("msg", status.message());
  EXPECT_EQ("INVALID_ARGUMENT:msg", status.ToString());
  status = Status(error::OK, "msg");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("", status.error_message());
  EXPECT_EQ("", status.message());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, Copy) {
  Status a(error::UNKNOWN, "message");
  Status b(a);
  EXPECT_EQ(a.ToString(), b.ToString());
}

TEST(Status, Assign) {
  Status a(error::UNKNOWN, "message");
  Status b;
  b = a;
  EXPECT_EQ(a.ToString(), b.ToString());
}

TEST(Status, AssignEmpty) {
  Status a(error::UNKNOWN, "message");
  Status b;
  a = b;
  EXPECT_EQ(std::string("OK"), a.ToString());
  EXPECT_TRUE(b.ok());
  EXPECT_TRUE(a.ok());
}

TEST(Status, EqualsOK) {
  EXPECT_EQ(Status::StatusOK(), Status());
}

TEST(Status, EqualsSame) {
  const Status a = Status(error::CANCELLED, "message");
  const Status b = Status(error::CANCELLED, "message");
  EXPECT_EQ(a, b);
}

TEST(Status, EqualsCopy) {
  const Status a = Status(error::CANCELLED, "message");
  const Status b = a;
  EXPECT_EQ(a, b);
}

TEST(Status, EqualsDifferentCode) {
  const Status a = Status(error::CANCELLED, "message");
  const Status b = Status(error::UNKNOWN, "message");
  EXPECT_NE(a, b);
}

TEST(Status, EqualsDifferentMessage) {
  const Status a = Status(error::CANCELLED, "message");
  const Status b = Status(error::CANCELLED, "another");
  EXPECT_NE(a, b);
}
}  // namespace
}  // namespace reporting

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/status.h"

#include <stdio.h>

#include "chrome/browser/policy/messaging_layer/util/status.pb.h"
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

TEST(Status, SaveOkTo) {
  StatusProto status_proto;
  Status::StatusOK().SaveTo(&status_proto);

  EXPECT_EQ(status_proto.code(), error::OK);
  EXPECT_TRUE(status_proto.error_message().empty())
      << status_proto.error_message();
}

TEST(Status, SaveTo) {
  Status status(error::INVALID_ARGUMENT, "argument error");
  StatusProto status_proto;
  status.SaveTo(&status_proto);

  EXPECT_EQ(status_proto.code(), status.error_code());
  EXPECT_EQ(status_proto.error_message(), status.error_message());
}

TEST(Status, RestoreFromOk) {
  StatusProto status_proto;
  status_proto.set_code(error::OK);
  status_proto.set_error_message("invalid error");

  Status status;
  status.RestoreFrom(status_proto);

  EXPECT_EQ(status.error_code(), status_proto.code());
  // Error messages are ignored for OK status objects.
  EXPECT_TRUE(status.error_message().empty()) << status.error_message();
  EXPECT_TRUE(status.ok());
}

TEST(Status, RestoreFromNonOk) {
  StatusProto status_proto;
  status_proto.set_code(error::INVALID_ARGUMENT);
  status_proto.set_error_message("argument error");

  Status status;
  status.RestoreFrom(status_proto);

  EXPECT_EQ(status.error_code(), status_proto.code());
  EXPECT_EQ(status.error_message(), status_proto.error_message());
}
}  // namespace
}  // namespace reporting

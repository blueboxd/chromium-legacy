// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"

#include <stdio.h>

#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using policy::DMToken;
using reporting_messaging_layer::Destination;
using reporting_messaging_layer::Priority;

// Tests to ensure that only valid parameters are used to generate a
// ReportQueueConfiguration.
TEST(ReportQueueConfiguration, ParametersMustBeValid) {
  // Invalid Parameters
  const DMToken invalid_dm_token = DMToken::CreateInvalidTokenForTesting();
  const Destination invalid_destination = Destination::UNDEFINED_DESTINATION;
  const Priority invalid_priority = Priority::UNDEFINED_PRIORITY;

  constexpr char kDmToken[] = "dm_token";

  // Valid Parameters
  const DMToken valid_dm_token = DMToken::CreateValidTokenForTesting(kDmToken);
  const Destination valid_destination = Destination::UPLOAD_EVENTS;
  const Priority valid_priority = Priority::IMMEDIATE;

  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   invalid_dm_token, valid_destination, valid_priority)
                   .ok());
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   valid_dm_token, invalid_destination, valid_priority)
                   .ok());
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   valid_dm_token, valid_destination, invalid_priority)
                   .ok());

  EXPECT_TRUE(ReportQueueConfiguration::Create(
                  valid_dm_token, valid_destination, valid_priority)
                  .ok());
}

}  // namespace
}  // namespace reporting

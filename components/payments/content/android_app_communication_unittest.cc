// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

TEST(AndroidAppCommunicationTest, SmokeTest) {
  auto support = AndroidAppCommunicationTestSupport::Create();
  auto scoped_initialization = support->CreateScopedInitialization();
  support->ExpectNoListOfPaymentAppsQuery();
  support->ExpectNoIsReadyToPayQuery();
  support->ExpectNoPaymentAppInvoke();
}

}  // namespace
}  // namespace payments

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "components/payments/content/android_app_communication_test_support.h"
#include "components/payments/core/android_app_description.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

std::vector<std::unique_ptr<AndroidAppDescription>> createApp(
    const std::vector<std::string>& activity_names,
    const std::string& default_payment_method,
    const std::vector<std::string>& service_names) {
  auto app = std::make_unique<AndroidAppDescription>();

  for (const auto& activity_name : activity_names) {
    auto activity = std::make_unique<AndroidActivityDescription>();
    activity->name = activity_name;
    activity->default_payment_method = default_payment_method;
    app->activities.emplace_back(std::move(activity));
  }

  app->package = "com.example.app";
  app->service_names = service_names;

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::move(app));

  return apps;
}

class AndroidAppCommunicationTest : public testing::Test {
 public:
  AndroidAppCommunicationTest()
      : support_(AndroidAppCommunicationTestSupport::Create()) {}
  ~AndroidAppCommunicationTest() override = default;

  AndroidAppCommunicationTest(const AndroidAppCommunicationTest& other) =
      delete;
  AndroidAppCommunicationTest& operator=(
      const AndroidAppCommunicationTest& other) = delete;

  void OnGetAppDescriptionsResponse(
      const base::Optional<std::string>& error,
      std::vector<std::unique_ptr<AndroidAppDescription>> apps) {
    error_ = error;
    apps_ = std::move(apps);
  }

  void OnIsReadyToPayResponse(const base::Optional<std::string>& error,
                              bool is_ready_to_pay) {
    error_ = error;
    is_ready_to_pay_ = is_ready_to_pay;
  }

  std::unique_ptr<AndroidAppCommunicationTestSupport> support_;
  base::Optional<std::string> error_;
  std::vector<std::unique_ptr<AndroidAppDescription>> apps_;
  bool is_ready_to_pay_ = false;
};

TEST_F(AndroidAppCommunicationTest, OneInstancePerBrowserContext) {
  auto communication_one =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  auto communication_two =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  EXPECT_EQ(communication_one.get(), communication_two.get());
}

TEST_F(AndroidAppCommunicationTest, NoArcForGetAppDescriptions) {
  // Intentionally do not set an instance.

  support_->ExpectNoListOfPaymentAppsQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  } else {
    EXPECT_FALSE(error_.has_value());
  }

  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, NoAppDescriptions) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond({});

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());
  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, TwoActivitiesInPackage) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.ActivityOne", "com.example.app.ActivityTwo"},
                "https://play.google.com/billing", {}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ(
        "Found more than one PAY activity in the Trusted Web Activity, but at "
        "most one activity is supported.",
        error_.value());
  } else {
    EXPECT_FALSE(error_.has_value());
  }
  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, TwoServicesInPackage) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.Activity"}, "https://play.google.com/billing",
                {"com.example.app.ServiceOne", "com.example.app.ServiceTwo"}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ(
        "Found more than one IS_READY_TO_PAY service in the Trusted Web "
        "Activity, but at most one service is supported.",
        error_.value());
  } else {
    EXPECT_FALSE(error_.has_value());
  }

  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, ActivityAndService) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.Activity"}, "https://play.google.com/billing",
                {"com.example.app.Service"}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_EQ(1u, apps_.size());
    ASSERT_NE(nullptr, apps_.front().get());
    EXPECT_EQ("com.example.app", apps_.front()->package);
    EXPECT_EQ(std::vector<std::string>{"com.example.app.Service"},
              apps_.front()->service_names);
    ASSERT_EQ(1u, apps_.front()->activities.size());
    ASSERT_NE(nullptr, apps_.front()->activities.front().get());
    EXPECT_EQ("com.example.app.Activity",
              apps_.front()->activities.front()->name);
    EXPECT_EQ("https://play.google.com/billing",
              apps_.front()->activities.front()->default_payment_method);
  } else {
    EXPECT_TRUE(apps_.empty());
  }
}

TEST_F(AndroidAppCommunicationTest, OnlyActivity) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(createApp(
      {"com.example.app.Activity"}, "https://play.google.com/billing", {}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_EQ(1u, apps_.size());
    ASSERT_NE(nullptr, apps_.front().get());
    EXPECT_EQ("com.example.app", apps_.front()->package);
    EXPECT_TRUE(apps_.front()->service_names.empty());
    ASSERT_EQ(1u, apps_.front()->activities.size());
    ASSERT_NE(nullptr, apps_.front()->activities.front().get());
    EXPECT_EQ("com.example.app.Activity",
              apps_.front()->activities.front()->name);
    EXPECT_EQ("https://play.google.com/billing",
              apps_.front()->activities.front()->default_payment_method);
  } else {
    EXPECT_TRUE(apps_.empty());
  }
}

TEST_F(AndroidAppCommunicationTest, OutsideOfTwa) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoListOfPaymentAppsQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      /*twa_package_name=*/"",  // Empty string means this is not TWA.
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());
  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, NoArcForIsReadyToPay) {
  // Intentionally do not set an instance.

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  EXPECT_FALSE(is_ready_to_pay_);
}

TEST_F(AndroidAppCommunicationTest, TwaIsReadyToPayOnlyWithPlayBilling) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://example.com"].insert("{}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }

  EXPECT_FALSE(is_ready_to_pay_);
}

TEST_F(AndroidAppCommunicationTest, MoreThanOnePaymentMethodDataNotReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"1\"}");
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"2\"}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  ASSERT_TRUE(error_.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_EQ("At most one payment method specific data is supported.",
              error_.value());
  } else {
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }

  EXPECT_FALSE(is_ready_to_pay_);
}

TEST_F(AndroidAppCommunicationTest, EmptyMethodDataIsReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(true);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data.insert(std::make_pair(
      "https://play.google.com/billing", std::set<std::string>()));
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
    EXPECT_TRUE(is_ready_to_pay_);
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
    EXPECT_FALSE(is_ready_to_pay_);
  }
}

TEST_F(AndroidAppCommunicationTest, NotReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(false);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }

  EXPECT_FALSE(is_ready_to_pay_);
}

TEST_F(AndroidAppCommunicationTest, ReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(true);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
    EXPECT_TRUE(is_ready_to_pay_);
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
    EXPECT_FALSE(is_ready_to_pay_);
  }
}

}  // namespace
}  // namespace payments

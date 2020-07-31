// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/pay/arc_payment_app_bridge.h"

#include <utility>

#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/test_browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class MockPaymentAppInstance : public mojom::PaymentAppInstance {
 public:
  MOCK_METHOD2(
      IsPaymentImplemented,
      void(const std::string& package_name,
           ArcPaymentAppBridge::IsPaymentImplementedCallback callback));
};

class ArcPaymentAppBridgeTest : public testing::Test {
 public:
  ArcPaymentAppBridgeTest() = default;
  ~ArcPaymentAppBridgeTest() override = default;

  ArcPaymentAppBridgeTest(const ArcPaymentAppBridgeTest& other) = delete;
  ArcPaymentAppBridgeTest& operator=(const ArcPaymentAppBridgeTest& other) =
      delete;

  void OnPaymentImplementedResponse(
      mojom::IsPaymentImplementedResultPtr response) {
    is_implemented_ = std::move(response);
  }

  // The |manager_| must be used on the same thread as where it was created, so
  // create it in the test instead of using ArcServiceManager::Get().
  ArcServiceManager manager_;
  MockPaymentAppInstance instance_;

  // Required for the TestBrowserContext.
  content::BrowserTaskEnvironment task_environment_;

  // Used for retrieving an instance of ArcPaymentAppBridge owned by a
  // BrowserContext.
  TestBrowserContext context_;

  mojom::IsPaymentImplementedResultPtr is_implemented_;
};

class ScopedSetInstance {
 public:
  ScopedSetInstance(ArcServiceManager* manager,
                    MockPaymentAppInstance* instance)
      : manager_(manager), instance_(instance) {
    manager_->arc_bridge_service()->payment_app()->SetInstance(instance_);
  }

  ~ScopedSetInstance() {
    manager_->arc_bridge_service()->payment_app()->CloseInstance(instance_);
  }

 private:
  ArcServiceManager* manager_;
  MockPaymentAppInstance* instance_;
};

TEST_F(ArcPaymentAppBridgeTest, UnableToConnectInIsImplemented) {
  // Intentionally do not set an instance.

  EXPECT_CALL(instance_, IsPaymentImplemented(testing::_, testing::_)).Times(0);

  ArcPaymentAppBridge::GetForBrowserContextForTesting(&context_)
      ->IsPaymentImplemented(
          "com.example.app",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnPaymentImplementedResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_implemented_.is_null());
  EXPECT_FALSE(is_implemented_->is_valid());
  ASSERT_TRUE(is_implemented_->is_error());
  EXPECT_EQ("Unable to invoke Android apps.", is_implemented_->get_error());
}

TEST_F(ArcPaymentAppBridgeTest, IsImplemented) {
  ScopedSetInstance scoped_set_instance(&manager_, &instance_);

  EXPECT_CALL(instance_, IsPaymentImplemented(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const std::string& package_name,
             ArcPaymentAppBridge::IsPaymentImplementedCallback callback) {
            auto valid = mojom::IsPaymentImplementedValidResult::New();
            valid->activity_names.push_back("com.example.Activity");
            valid->service_names.push_back("com.example.Service");
            std::move(callback).Run(
                mojom::IsPaymentImplementedResult::NewValid(std::move(valid)));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(&context_)
      ->IsPaymentImplemented(
          "com.example.app",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnPaymentImplementedResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_implemented_.is_null());
  EXPECT_FALSE(is_implemented_->is_error());
  ASSERT_TRUE(is_implemented_->is_valid());
  ASSERT_FALSE(is_implemented_->get_valid().is_null());
  EXPECT_EQ(std::vector<std::string>{"com.example.Activity"},
            is_implemented_->get_valid()->activity_names);
  EXPECT_EQ(std::vector<std::string>{"com.example.Service"},
            is_implemented_->get_valid()->service_names);
}

TEST_F(ArcPaymentAppBridgeTest, IsNotImplemented) {
  ScopedSetInstance scoped_set_instance(&manager_, &instance_);

  EXPECT_CALL(instance_, IsPaymentImplemented(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const std::string& package_name,
             ArcPaymentAppBridge::IsPaymentImplementedCallback callback) {
            std::move(callback).Run(mojom::IsPaymentImplementedResult::NewValid(
                mojom::IsPaymentImplementedValidResult::New()));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(&context_)
      ->IsPaymentImplemented(
          "com.example.app",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnPaymentImplementedResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_implemented_.is_null());
  EXPECT_FALSE(is_implemented_->is_error());
  ASSERT_TRUE(is_implemented_->is_valid());
  ASSERT_FALSE(is_implemented_->get_valid().is_null());
  EXPECT_TRUE(is_implemented_->get_valid()->activity_names.empty());
  EXPECT_TRUE(is_implemented_->get_valid()->service_names.empty());
}

TEST_F(ArcPaymentAppBridgeTest, ImplementationCheckError) {
  ScopedSetInstance scoped_set_instance(&manager_, &instance_);

  EXPECT_CALL(instance_, IsPaymentImplemented(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const std::string& package_name,
             ArcPaymentAppBridge::IsPaymentImplementedCallback callback) {
            std::move(callback).Run(
                mojom::IsPaymentImplementedResult::NewError("Error message."));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(&context_)
      ->IsPaymentImplemented(
          "com.example.app",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnPaymentImplementedResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_implemented_.is_null());
  EXPECT_FALSE(is_implemented_->is_valid());
  ASSERT_TRUE(is_implemented_->is_error());
  EXPECT_EQ("Error message.", is_implemented_->get_error());
}

}  // namespace
}  // namespace arc

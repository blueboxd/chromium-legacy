// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::PasswordManagerInternalsService;
using password_manager::PasswordManagerInternalsServiceFactory;

namespace {

const char kTestText[] = "abcd1234";

class MockLogReceiver : public password_manager::LogReceiver {
 public:
  MockLogReceiver() {}

  MOCK_METHOD1(LogSavePasswordProgress, void(const std::string&));
};

}  // namespace

class PasswordManagerInternalsServiceFactoryTest : public testing::Test {
 public:
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestBrowserContext browser_context_;

  void SetUp() override {
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        &browser_context_);
  }

  void TearDown() override {
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(&browser_context_);
  }
};

// When the profile is not incognito, it should be possible to activate the
// service.
TEST_F(PasswordManagerInternalsServiceFactoryTest, ServiceActiveNonIncognito) {
  browser_context_.set_is_off_the_record(false);
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(
          &browser_context_);
  testing::StrictMock<MockLogReceiver> receiver;

  ASSERT_TRUE(service);
  EXPECT_EQ(std::string(), service->RegisterReceiver(&receiver));

  EXPECT_CALL(receiver, LogSavePasswordProgress(kTestText)).Times(1);
  service->ProcessLog(kTestText);

  service->UnregisterReceiver(&receiver);
}

// When the browser profile is incognito, it should not be possible to activate
// the service.
TEST_F(PasswordManagerInternalsServiceFactoryTest, ServiceNotActiveIncognito) {
  browser_context_.set_is_off_the_record(true);
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(
          &browser_context_);
  // BrowserContextKeyedServiceFactory::GetBrowserContextToUse should return
  // nullptr for |browser_context|, because |browser_context| is incognito.
  // Therefore the returned |service| should also be nullptr.
  EXPECT_FALSE(service);
}

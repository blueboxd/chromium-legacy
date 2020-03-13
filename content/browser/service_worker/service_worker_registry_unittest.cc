// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registry.h"

#include "base/test/bind_test_util.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerRegistryTest : public testing::Test {
 public:
  ServiceWorkerRegistryTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    CHECK(user_data_directory_.CreateUniqueTempDir());
    user_data_directory_path_ = user_data_directory_.GetPath();
    special_storage_policy_ =
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(
        user_data_directory_path_, special_storage_policy_.get());
    registry()->storage()->LazyInitializeForTest();
  }

  void TearDown() override {
    helper_.reset();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerRegistry* registry() { return context()->registry(); }

  storage::MockSpecialStoragePolicy* special_storage_policy() {
    return special_storage_policy_.get();
  }

  blink::ServiceWorkerStatusCode StoreRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->StoreRegistration(
        registration.get(), version.get(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

 private:
  // |user_data_directory_| must be declared first to preserve destructor order.
  base::ScopedTempDir user_data_directory_;
  base::FilePath user_data_directory_path_;

  BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
};

// Tests that storage policy changes are observed.
TEST_F(ServiceWorkerRegistryTest, StoragePolicyChange) {
  const GURL kScope("http://www.example.com/scope/");
  const GURL kScriptUrl("http://www.example.com/script.js");
  const auto kOrigin(url::Origin::Create(kScope));

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScriptUrl,
                                                /*resource_id=*/1);

  ASSERT_EQ(StoreRegistration(registration, registration->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_FALSE(registry()->ShouldPurgeOnShutdown(kOrigin));

  {
    // Update storage policy to mark the origin should be purged on shutdown.
    special_storage_policy()->AddSessionOnly(kOrigin.GetURL());
    special_storage_policy()->NotifyPolicyChanged();
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(registry()->ShouldPurgeOnShutdown(kOrigin));
}

}  // namespace content

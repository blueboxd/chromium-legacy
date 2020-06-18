// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

// Fixture for the ServiceWorkerContextWrapper test. It uses a disk user data
// directory in order to test starting the browser with a registration already
// written to storage.
class ServiceWorkerContextWrapperTest : public testing::Test {
 public:
  ServiceWorkerContextWrapperTest() = default;

  void SetUp() override {
    ASSERT_TRUE(user_data_directory_.CreateUniqueTempDir());
    InitWrapper();
  }

  void TearDown() override {
    // Shutdown or else ASAN complains of leaks.
    wrapper_->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void InitWrapper() {
    browser_context_ = std::make_unique<TestBrowserContext>();
    wrapper_ = base::MakeRefCounted<ServiceWorkerContextWrapper>(
        browser_context_.get());
    url_loader_factory_getter_ = base::MakeRefCounted<URLLoaderFactoryGetter>();
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(
            BrowserContext::GetStoragePartitionForSite(
                browser_context_.get(), GURL("https://example.com")));
    wrapper_->set_storage_partition(storage_partition);
    wrapper_->Init(user_data_directory_.GetPath(), nullptr, nullptr, nullptr,
                   url_loader_factory_getter_.get());
    // Init() posts a couple tasks to the IO thread. Let them finish.
    base::RunLoop().RunUntilIdle();

    storage()->LazyInitializeForTest();
  }

  ServiceWorkerContextCore* context() { return wrapper_->context(); }
  ServiceWorkerRegistry* registry() { return context()->registry(); }
  ServiceWorkerStorage* storage() { return context()->storage(); }

 protected:
  BrowserTaskEnvironment task_environment_{BrowserTaskEnvironment::IO_MAINLOOP};
  base::ScopedTempDir user_data_directory_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextWrapperTest);
};

// Test that the UI thread knows which origins have registrations upon
// browser startup. Regression test for https://crbug.com/991143.
TEST_F(ServiceWorkerContextWrapperTest, HasRegistration) {
  // Make a service worker.
  GURL scope("https://example.com/");
  GURL script("https://example.com/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), scope, script,
                                                /*resource_id=*/1);

  // Store it.
  base::RunLoop loop;
  registry()->StoreRegistration(
      registration.get(), registration->waiting_version(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) {
            ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
            loop.Quit();
          }));
  loop.Run();

  // Simulate browser shutdown and restart.
  wrapper_->Shutdown();
  base::RunLoop().RunUntilIdle();
  wrapper_.reset();
  InitWrapper();

  // Now test that registrations are recognized.
  wrapper_->WaitForRegistrationsInitializedForTest();
  EXPECT_TRUE(wrapper_->HasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example.com"))));
  EXPECT_FALSE(wrapper_->HasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example.org"))));
}

// This test involves storing two registrations for the same origin to storage
// and deleting one of them to check that HasRegistrationForOrigin still
// correctly returns TRUE since there is still one registration for the origin,
// and should only return FALSE when ALL registrations for that origin have been
// deleted from storage.
TEST_F(ServiceWorkerContextWrapperTest, DeleteRegistrationsForSameOrigin) {
  wrapper_->WaitForRegistrationsInitializedForTest();

  // Make two registrations for same origin.
  GURL scope1("https://example1.com/abc/");
  GURL script1("https://example1.com/abc/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope1, script1,
                                                /*resource_id=*/1);
  GURL scope2("https://example1.com/xyz/");
  GURL script2("https://example1.com/xyz/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope2, script2, 1);

  // Store both registrations.
  base::RunLoop loop1;
  registry()->StoreRegistration(
      registration1.get(), registration1->waiting_version(),
      base::BindLambdaForTesting(
          [&loop1](blink::ServiceWorkerStatusCode status1) {
            ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status1);
            loop1.Quit();
          }));
  loop1.Run();

  base::RunLoop loop2;
  registry()->StoreRegistration(
      registration2.get(), registration2->waiting_version(),
      base::BindLambdaForTesting(
          [&loop2](blink::ServiceWorkerStatusCode status2) {
            ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status2);
            loop2.Quit();
          }));
  loop2.Run();

  // Delete one of the registrations.
  base::RunLoop loop3;
  registry()->DeleteRegistration(
      registration1.get(), registration1->scope().GetOrigin(),
      base::BindLambdaForTesting(
          [&loop3](blink::ServiceWorkerStatusCode status3) {
            ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status3);
            loop3.Quit();
          }));
  loop3.Run();

  // Run loop until idle to wait for
  // ServiceWorkerRegistry::DidDeleteRegistration() to be executed, and make
  // sure that NotifyAllRegistrationsDeletedForOrigin() is not called.
  base::RunLoop().RunUntilIdle();

  // Now test that a registration for an origin is still recognized.
  EXPECT_TRUE(wrapper_->HasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example1.com"))));

  // Remove second registration.
  base::RunLoop loop4;
  registry()->DeleteRegistration(
      registration2.get(), registration2->scope().GetOrigin(),
      base::BindLambdaForTesting(
          [&loop4](blink::ServiceWorkerStatusCode status4) {
            ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status4);
            loop4.Quit();
          }));
  loop4.Run();

  // Run loop until idle to wait for
  // ServiceWorkerRegistry::DidDeleteRegistration() to be executed, and make
  // sure that this time NotifyAllRegistrationsDeletedForOrigin() is called.
  base::RunLoop().RunUntilIdle();

  // Now test that origin does not have any registrations.
  EXPECT_FALSE(wrapper_->HasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example1.com"))));
}

// This tests deleting registrations from storage and checking that even if live
// registrations may exist, HasRegistrationForOrigin correctly returns FALSE
// since the registrations do not exist in storage.
TEST_F(ServiceWorkerContextWrapperTest, DeleteRegistration) {
  wrapper_->WaitForRegistrationsInitializedForTest();

  // Make registration.
  GURL scope1("https://example2.com/");
  GURL script1("https://example2.com/");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), scope1, script1,
                                                /*resource_id=*/1);

  // Store registration.
  base::RunLoop loop1;
  registry()->StoreRegistration(
      registration.get(), registration->waiting_version(),
      base::BindLambdaForTesting(
          [&loop1](blink::ServiceWorkerStatusCode status1) {
            ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status1);
            loop1.Quit();
          }));
  loop1.Run();

  wrapper_->OnRegistrationCompleted(registration->id(), registration->scope());
  base::RunLoop().RunUntilIdle();

  // Now test that a registration is recognized.
  EXPECT_TRUE(wrapper_->HasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example2.com"))));

  // Delete registration from storage.
  base::RunLoop loop2;
  registry()->DeleteRegistration(
      registration.get(), registration->scope().GetOrigin(),
      base::BindLambdaForTesting(
          [&loop2](blink::ServiceWorkerStatusCode status2) {
            ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status2);
            loop2.Quit();
          }));
  loop2.Run();

  // Finish deleting registration from storage.
  base::RunLoop().RunUntilIdle();

  // Now test that origin does not have any registrations. This should return
  // FALSE even when live registrations may exist, as the registrations have
  // been deleted from storage.
  EXPECT_FALSE(wrapper_->HasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example2.com"))));
}
}  // namespace content

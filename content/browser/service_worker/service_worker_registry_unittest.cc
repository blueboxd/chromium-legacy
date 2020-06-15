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

namespace {

void FindCallback(base::OnceClosure quit_closure,
                  base::Optional<blink::ServiceWorkerStatusCode>* result,
                  scoped_refptr<ServiceWorkerRegistration>* found,
                  blink::ServiceWorkerStatusCode status,
                  scoped_refptr<ServiceWorkerRegistration> registration) {
  *result = status;
  *found = std::move(registration);
  std::move(quit_closure).Run();
}

}  // namespace

class ServiceWorkerRegistryTest : public testing::Test {
 public:
  ServiceWorkerRegistryTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    CHECK(user_data_directory_.CreateUniqueTempDir());
    user_data_directory_path_ = user_data_directory_.GetPath();
    special_storage_policy_ =
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    InitializeTestHelper();
    LazyInitialize();
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

  void InitializeTestHelper() {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(
        user_data_directory_path_, special_storage_policy_.get());
  }

  void LazyInitialize() { registry()->storage()->LazyInitializeForTest(); }

  void SimulateRestart() {
    // Need to reset |helper_| then wait for scheduled tasks to be finished
    // because |helper_| has TestBrowserContext and the dtor schedules storage
    // cleanup tasks.
    helper_.reset();
    base::RunLoop().RunUntilIdle();
    InitializeTestHelper();
    LazyInitialize();
  }

  blink::ServiceWorkerStatusCode FindRegistrationForClientUrl(
      const GURL& document_url,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->FindRegistrationForClientUrl(
        document_url, base::BindOnce(&FindCallback, loop.QuitClosure(), &result,
                                     registration));
    loop.Run();
    return result.value();
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

  blink::ServiceWorkerStatusCode GetAllRegistrationsInfos(
      std::vector<ServiceWorkerRegistrationInfo>* registrations) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->GetAllRegistrationsInfos(base::BindLambdaForTesting(
        [&](blink::ServiceWorkerStatusCode status,
            const std::vector<ServiceWorkerRegistrationInfo>& infos) {
          result = status;
          *registrations = infos;
          loop.Quit();
        }));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

 private:
  // |user_data_directory_| must be declared first to preserve destructor order.
  base::ScopedTempDir user_data_directory_;
  base::FilePath user_data_directory_path_;

  BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
};

TEST_F(ServiceWorkerRegistryTest, FindRegistration_LongestScopeMatch) {
  LazyInitialize();
  const GURL kDocumentUrl("http://www.example.com/scope/foo");
  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Registration for "/scope/".
  const GURL kScope1("http://www.example.com/scope/");
  const GURL kScript1("http://www.example.com/script1.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope1, kScript1,
                                                /*resource_id=*/1);

  // Registration for "/scope/foo".
  const GURL kScope2("http://www.example.com/scope/foo");
  const GURL kScript2("http://www.example.com/script2.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope2, kScript2,
                                                /*resource_id=*/2);

  // Registration for "/scope/foobar".
  const GURL kScope3("http://www.example.com/scope/foobar");
  const GURL kScript3("http://www.example.com/script3.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration3 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope3, kScript3,
                                                /*resource_id=*/3);

  // Notify storage of them being installed.
  registry()->NotifyInstallingRegistration(live_registration1.get());
  registry()->NotifyInstallingRegistration(live_registration2.get());
  registry()->NotifyInstallingRegistration(live_registration3.get());

  // Find a registration among installing ones.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
  found_registration = nullptr;

  // Store registrations.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration1,
                              live_registration1->waiting_version()));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration2,
                              live_registration2->waiting_version()));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration3,
                              live_registration3->waiting_version()));

  // Notify storage of installations no longer happening.
  registry()->NotifyDoneInstallingRegistration(
      live_registration1.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);
  registry()->NotifyDoneInstallingRegistration(
      live_registration2.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);
  registry()->NotifyDoneInstallingRegistration(
      live_registration3.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);

  // Find a registration among installed ones.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
}

// Tests that fields of ServiceWorkerRegistrationInfo are filled correctly.
TEST_F(ServiceWorkerRegistryTest, RegistrationInfoFields) {
  const GURL kScope("http://www.example.com/scope/");
  const GURL kScript("http://www.example.com/script1.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                /*resource_id=*/1);

  // Set some fields to check ServiceWorkerStorage serializes/deserializes
  // these fields correctly.
  registration->SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  registration->EnableNavigationPreload(true);
  registration->SetNavigationPreloadHeader("header");

  registry()->NotifyInstallingRegistration(registration.get());
  ASSERT_EQ(StoreRegistration(registration, registration->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(GetAllRegistrationsInfos(&all_registrations),
            blink::ServiceWorkerStatusCode::kOk);
  ASSERT_EQ(all_registrations.size(), 1UL);

  ServiceWorkerRegistrationInfo info = all_registrations[0];
  EXPECT_EQ(info.scope, registration->scope());
  EXPECT_EQ(info.update_via_cache, registration->update_via_cache());
  EXPECT_EQ(info.registration_id, registration->id());
  EXPECT_EQ(info.navigation_preload_enabled,
            registration->navigation_preload_state().enabled);
  EXPECT_EQ(info.navigation_preload_header_length,
            registration->navigation_preload_state().header.size());
}

// Tests loading a registration with an enabled navigation preload state, as
// well as a custom header value.
TEST_F(ServiceWorkerRegistryTest, EnabledNavigationPreloadState) {
  const GURL kScope("https://valid.example.com/scope");
  const GURL kScript("https://valid.example.com/script.js");
  const std::string kHeaderValue("custom header value");

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                /*resource_id=*/1);
  ServiceWorkerVersion* version = registration->waiting_version();
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration->SetActiveVersion(version);
  registration->EnableNavigationPreload(true);
  registration->SetNavigationPreloadHeader(kHeaderValue);

  ASSERT_EQ(StoreRegistration(registration, version),
            blink::ServiceWorkerStatusCode::kOk);

  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  SimulateRestart();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(FindRegistrationForClientUrl(kScope, &found_registration),
            blink::ServiceWorkerStatusCode::kOk);
  const blink::mojom::NavigationPreloadState& registration_state =
      found_registration->navigation_preload_state();
  EXPECT_TRUE(registration_state.enabled);
  EXPECT_EQ(registration_state.header, kHeaderValue);
  ASSERT_TRUE(found_registration->active_version());
  const blink::mojom::NavigationPreloadState& state =
      found_registration->active_version()->navigation_preload_state();
  EXPECT_TRUE(state.enabled);
  EXPECT_EQ(state.header, kHeaderValue);
}

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

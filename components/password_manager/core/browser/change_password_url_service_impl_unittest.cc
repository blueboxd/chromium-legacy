// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/change_password_url_service_impl.h"

#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kMockResponse[] = R"({
  "google.com": "https://google.com/change-password",
  "a.netlify.com": "https://a.netlify.com/change-password",
  "web.app": "https://web.app/change-password"
})";
}  // namespace

namespace password_manager {

class ChangePasswordUrlServiceTest : public testing::Test {
 public:
  ChangePasswordUrlServiceTest() {
    test_url_loader_factory_.AddResponse(
        password_manager::ChangePasswordUrlServiceImpl::
            kChangePasswordUrlOverrideUrl,
        kMockResponse);
    // Password Manager is enabled by default.
    test_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
  }

  // Test the bevahiour for a given |url| and compares the result to the given
  // |expected_url| in the callback.
  void TestOverride(GURL url, GURL expected_url);

  void DisablePasswordManagerEnabledPolicy() {
    test_pref_service_.SetBoolean(
        password_manager::prefs::kCredentialsEnableService, false);
  }

  void ClearMockResponses() { test_url_loader_factory_.ClearResponses(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  TestingPrefServiceSimple test_pref_service_;
  ChangePasswordUrlServiceImpl change_password_url_service_{
      test_shared_loader_factory_, &test_pref_service_};
};

void ChangePasswordUrlServiceTest::TestOverride(GURL url, GURL expected_url) {
  change_password_url_service_.Initialize();

  base::MockCallback<password_manager::ChangePasswordUrlService::UrlCallback>
      callback;
  EXPECT_CALL(callback, Run(expected_url));
  change_password_url_service_.GetChangePasswordUrl(url::Origin::Create(url),
                                                    callback.Get());
  // Retry option is set to 3 times with timeout of 3s -> 9s. One added second
  // is no problem because the |task_environment_| is still executing in the
  // correct order and does not skip tasks.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  task_environment_.RunUntilIdle();
}

TEST_F(ChangePasswordUrlServiceTest, eTLDLookup) {
  // TODO(crbug.com/1086141): If possible mock eTLD registry to ensure sites are
  // listed.
  TestOverride(GURL("https://google.com/foo"),
               GURL("https://google.com/change-password"));
  TestOverride(GURL("https://a.google.com/foo"),
               GURL("https://google.com/change-password"));

  TestOverride(GURL("https://web.app"), GURL("https://web.app"));

  TestOverride(GURL("https://netlify.com"), GURL("https://netlify.com"));
  TestOverride(GURL("https://a.netlify.com"),
               GURL("https://a.netlify.com/change-password"));
  TestOverride(GURL("https://b.netlify.com"), GURL("https://b.netlify.com"));

  TestOverride(GURL("https://notlisted.com/foo"),
               GURL("https://notlisted.com"));
}

TEST_F(ChangePasswordUrlServiceTest, PassworManagerPolicyDisabled) {
  DisablePasswordManagerEnabledPolicy();

  TestOverride(GURL("https://google.com/foo"), GURL("https://google.com/"));
}

TEST_F(ChangePasswordUrlServiceTest, NetworkRequestFails) {
  ClearMockResponses();

  TestOverride(GURL("https://google.com/foo"), GURL("https://google.com/"));
}

}  // namespace password_manager

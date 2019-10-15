// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_test_helper.h"
#include "chrome/browser/chromeos/extensions/autotest_private/autotest_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/connection_holder.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/views/widget/widget.h"

using testing::_;
using testing::Return;

namespace extensions {

class AutotestPrivateApiTest : public ExtensionApiTest {
 public:
  AutotestPrivateApiTest() = default;
  ~AutotestPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Make ARC enabled for tests.
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // Turn on testing mode so we don't kill the browser.
    AutotestPrivateAPI::GetFactoryInstance()
        ->Get(browser()->profile())
        ->set_test_mode(true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutotestPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, AutotestPrivate) {
  ASSERT_TRUE(RunComponentExtensionTestWithArg("autotest_private", "default"))
      << message_;
}

// Set of tests where ARC is enabled and test apps and packages are registered.
IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, AutotestPrivateArcEnabled) {
  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);

  arc::SetArcPlayStoreEnabledForProfile(profile(), true);
  // Provisioning is completed.
  browser()->profile()->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);
  browser()->profile()->GetPrefs()->SetBoolean(arc::prefs::kArcTermsAccepted,
                                               true);

  std::unique_ptr<arc::FakeAppInstance> app_instance;
  app_instance.reset(new arc::FakeAppInstance(prefs));
  prefs->app_connection_holder()->SetInstance(app_instance.get());
  arc::WaitForInstanceReady(prefs->app_connection_holder());

  arc::mojom::AppInfo app;
  app.name = "Fake App";
  app.package_name = "fake.package";
  app.activity = "fake.package.activity";
  app_instance->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(1, app));

  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.emplace_back(arc::mojom::ArcPackageInfo::New(
      app.package_name, 10 /* package_version */,
      100 /* last_backup_android_id */,
      base::Time::Now()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds() /* last_backup_time */,
      true /* sync */));
  app_instance->SendRefreshPackageList(std::move(packages));

  ASSERT_TRUE(
      RunComponentExtensionTestWithArg("autotest_private", "arcEnabled"))
      << message_;

  arc::SetArcPlayStoreEnabledForProfile(profile(), false);
}

class AutotestPrivateWithPolicyApiTest : public AutotestPrivateApiTest {
 public:
  AutotestPrivateWithPolicyApiTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    AutotestPrivateApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    AutotestPrivateApiTest::SetUpOnMainThread();
    // Set a fake policy
    policy::PolicyMap policy;
    policy.Set(policy::key::kAllowDinosaurEasterEgg,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true),
               nullptr);
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  policy::MockConfigurationPolicyProvider provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutotestPrivateWithPolicyApiTest);
};

// GetAllEnterprisePolicies Sanity check.
IN_PROC_BROWSER_TEST_F(AutotestPrivateWithPolicyApiTest, PolicyAPITest) {
  ASSERT_TRUE(RunComponentExtensionTestWithArg("autotest_private",
                                               "enterprisePolicies"))
      << message_;
}

class AutotestPrivateArcPerformanceTracing : public AutotestPrivateApiTest {
 public:
  AutotestPrivateArcPerformanceTracing() = default;
  ~AutotestPrivateArcPerformanceTracing() override = default;

 protected:
  // AutotestPrivateApiTest:
  void SetUpOnMainThread() override {
    AutotestPrivateApiTest::SetUpOnMainThread();
    tracing_helper_.SetUp(profile());
    performance_tracing()->SetCustomSessionReadyCallbackForTesting(
        base::BindRepeating(
            &arc::ArcAppPerformanceTracingTestHelper::PlayDefaultSequence,
            base::Unretained(&tracing_helper())));
  }

  void TearDownOnMainThread() override {
    performance_tracing()->SetCustomSessionReadyCallbackForTesting(
        arc::ArcAppPerformanceTracing::CustomSessionReadyCallback());
    tracing_helper_.TearDown();
    AutotestPrivateApiTest::TearDownOnMainThread();
  }

  arc::ArcAppPerformanceTracingTestHelper& tracing_helper() {
    return tracing_helper_;
  }

  arc::ArcAppPerformanceTracing* performance_tracing() {
    return tracing_helper_.GetTracing();
  }

 private:
  arc::ArcAppPerformanceTracingTestHelper tracing_helper_;

  DISALLOW_COPY_AND_ASSIGN(AutotestPrivateArcPerformanceTracing);
};

IN_PROC_BROWSER_TEST_F(AutotestPrivateArcPerformanceTracing, Basic) {
  views::Widget* const arc_widget =
      arc::ArcAppPerformanceTracingTestHelper::CreateArcWindow(
          "org.chromium.arc.1");
  performance_tracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());

  ASSERT_TRUE(RunComponentExtensionTestWithArg("autotest_private",
                                               "arcPerformanceTracing"))
      << message_;
}

}  // namespace extensions

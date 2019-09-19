// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Spins the loop until a notification is received from |prefs| that the value
// of |pref_name| has changed. If the notification is received before Wait()
// has been called, Wait() returns immediately and no loop is spun.
class PrefChangeWatcher {
 public:
  PrefChangeWatcher(const char* pref_name, PrefService* prefs);

  void Wait();

  void OnPrefChange();

 private:
  bool pref_changed_ = false;

  base::RunLoop run_loop_;
  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefChangeWatcher);
};

PrefChangeWatcher::PrefChangeWatcher(const char* pref_name,
                                     PrefService* prefs) {
  registrar_.Init(prefs);
  registrar_.Add(pref_name,
                 base::BindRepeating(&PrefChangeWatcher::OnPrefChange,
                                     base::Unretained(this)));
}

void PrefChangeWatcher::Wait() {
  if (!pref_changed_)
    run_loop_.Run();
}

void PrefChangeWatcher::OnPrefChange() {
  pref_changed_ = true;
  run_loop_.Quit();
}

}  // namespace

class LoginScreenAccessibilityPolicyBrowsertest
    : public DevicePolicyCrosBrowserTest {
 protected:
  LoginScreenAccessibilityPolicyBrowsertest();
  ~LoginScreenAccessibilityPolicyBrowsertest() override;

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void RefreshDevicePolicyAndWaitForPrefChange(const char* pref_name);

  // Returns the pref value only if it exists in the pref service and has the
  // same policy being applied as mandatory when |is_managed| is true or as
  // recommended when |is_managed| is false, otherwise returns an empty value.
  base::Value GetPrefValue(const char* pref_name, bool is_managed) const;

  Profile* login_profile_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenAccessibilityPolicyBrowsertest);
};

LoginScreenAccessibilityPolicyBrowsertest::
    LoginScreenAccessibilityPolicyBrowsertest() {}

LoginScreenAccessibilityPolicyBrowsertest::
    ~LoginScreenAccessibilityPolicyBrowsertest() {}

void LoginScreenAccessibilityPolicyBrowsertest::SetUpOnMainThread() {
  DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  login_profile_ = chromeos::ProfileHelper::GetSigninProfile();
  ASSERT_TRUE(login_profile_);
  // Set the login screen profile.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  accessibility_manager->SetProfileForTest(
      chromeos::ProfileHelper::GetSigninProfile());
}

void LoginScreenAccessibilityPolicyBrowsertest::
    RefreshDevicePolicyAndWaitForPrefChange(const char* pref_name) {
  PrefChangeWatcher watcher(pref_name, login_profile_->GetPrefs());
  RefreshDevicePolicy();
  watcher.Wait();
}

void LoginScreenAccessibilityPolicyBrowsertest::SetUpCommandLine(
    base::CommandLine* command_line) {
  DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
}

base::Value LoginScreenAccessibilityPolicyBrowsertest::GetPrefValue(
    const char* pref_name,
    bool is_managed) const {
  const PrefService::Preference* pref =
      login_profile_->GetPrefs()->FindPreference(pref_name);
  const bool is_recommended = pref && pref->IsRecommended() && !is_managed,
             is_mandatory = pref && pref->IsManaged() && is_managed;
  if (is_recommended || is_mandatory)
    return pref->GetValue()->Clone();
  else
    return base::Value();
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenLargeCursorEnabled) {
  // Verifies that the state of the large cursor accessibility feature on the
  // login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());

  // Manually enable the large cursor.
  accessibility_manager->EnableLargeCursor(true);
  EXPECT_TRUE(accessibility_manager->IsLargeCursorEnabled());

  // Disable the large cursor through device policy and wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_large_cursor_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the pref which controls the large cursor in the login profile
  // is managed by the policy.
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled,
                         /*is_managed=*/true));

  // Verify that the large cursor cannot be enabled manually anymore.
  accessibility_manager->EnableLargeCursor(true);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       LargeCursorEnabledOverridesDefaultPolicy) {
  // Verifies that the state of the large cursor accessibility feature on the
  // login screen will be controlled only through
  // DeviceLoginScreenLargeCursorEnabled device policy if both of
  // DeviceLoginScreenLargeCursorEnabled and
  // DeviceLoginScreenDefaultLargeCursorEnabled have been set.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());

  // Enable the large cursor through DeviceLoginScreenLargeCursorEnabled device
  // policy, and disable it through DeviceLoginScreenDefaultLargeCursorEnabled;
  // then wait for the change to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_large_cursor_enabled(
      true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_large_cursor_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_large_cursor_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the pref which controls the large cursor in the login profile
  // is managed by the policy and is enabled.
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled,
                         /*is_managed=*/false));

  // Disable the large cursor through DeviceLoginScreenLargeCursorEnabled device
  // policy, and enable it through DeviceLoginScreenDefaultLargeCursorEnabled;
  // then wait for the change to take effect.
  proto.mutable_accessibility_settings()->set_login_screen_large_cursor_enabled(
      false);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_large_cursor_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_large_cursor_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the pref which controls the large cursor in the login profile
  // is managed by the policy and is disabled.
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled,
                         /*is_managed=*/false));
}

}  // namespace policy

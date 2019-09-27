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

  bool IsPrefManaged(const char* pref_name) const;

  base::Value GetPrefValue(const char* pref_name) const;

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

bool LoginScreenAccessibilityPolicyBrowsertest::IsPrefManaged(
    const char* pref_name) const {
  const PrefService::Preference* pref =
      login_profile_->GetPrefs()->FindPreference(pref_name);
  return pref && pref->IsManaged();
}

base::Value LoginScreenAccessibilityPolicyBrowsertest::GetPrefValue(
    const char* pref_name) const {
  const PrefService::Preference* pref =
      login_profile_->GetPrefs()->FindPreference(pref_name);
  if (pref)
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
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled));

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
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled));

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
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenSpokenFeedbackEnabled) {
  // Verifies that the state of the spoken feedback accessibility feature on the
  // login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Manually enable the spoken feedback.
  accessibility_manager->EnableSpokenFeedback(true);
  EXPECT_TRUE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Disable the spoken feedback through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_spoken_feedback_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that the pref which controls the spoken feedback in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilitySpokenFeedbackEnabled));

  // Verify that the spoken feedback cannot be enabled manually anymore.
  accessibility_manager->EnableSpokenFeedback(true);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       SpokenFeedbackEnabledOverridesDefaultPolicy) {
  // Verifies that the state of the spoken feedback accessibility feature on the
  // login screen will be controlled only through
  // DeviceLoginScreenSpokenFeedbackEnabled device policy if both of
  // DeviceLoginScreenSpokenFeedbackEnabled and
  // DeviceLoginScreenDefaultSpokenFeedbackEnabled have been set.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Enable the spoken feedback through DeviceLoginScreenSpokenFeedbackEnabled
  // device policy, and disable it through
  // DeviceLoginScreenDefaultSpokenFeedbackEnabled; then wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_spoken_feedback_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_spoken_feedback_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_spoken_feedback_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that the pref which controls the spoken feedback in the login
  // profile is managed by the policy and is enabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilitySpokenFeedbackEnabled));

  // Disable the spoken feedback through DeviceLoginScreenSpokenFeedbackEnabled
  // device policy, and enable it through
  // DeviceLoginScreenDefaultSpokenFeedbackEnabled; then wait for the change to
  // take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_spoken_feedback_enabled(false);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_spoken_feedback_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_spoken_feedback_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that the pref which controls the spoken feedback in the login
  // profile is managed by the policy and is disabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenHighContrastEnabled) {
  // Verifies that the state of the high contrast accessibility feature on the
  // login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());

  // Manually enable the high contrast.
  accessibility_manager->EnableHighContrast(true);
  EXPECT_TRUE(accessibility_manager->IsHighContrastEnabled());

  // Disable the high contrast through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_high_contrast_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that the pref which controls the high contrast in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityHighContrastEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityHighContrastEnabled));

  // Verify that the high contrast cannot be enabled manually anymore.
  accessibility_manager->EnableHighContrast(true);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       HighContrastEnabledOverridesDefaultPolicy) {
  // Verifies that the state of the high contrast accessibility feature on the
  // login screen will be controlled only through
  // DeviceLoginScreenHighContrastEnabled device policy if both of
  // DeviceLoginScreenHighContrastEnabled and
  // DeviceLoginScreenDefaultHighContrastEnabled have been set.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());

  // Enable the high contrast through DeviceLoginScreenHighContrastEnabled
  // device policy, and disable it through
  // DeviceLoginScreenDefaultHighContrastEnabled; then wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_high_contrast_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_high_contrast_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_high_contrast_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that the pref which controls the high contrast in the login
  // profile is managed by the policy and is enabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityHighContrastEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityHighContrastEnabled));

  // Disable the high contrast through DeviceLoginScreenHighContrastEnabled
  // device policy, and enable it through
  // DeviceLoginScreenDefaultHighContrastEnabled; then wait for the change to
  // take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_high_contrast_enabled(false);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_high_contrast_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_high_contrast_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that the pref which controls the high contrast in the login
  // profile is managed by the policy and is disabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityHighContrastEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityHighContrastEnabled));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenVirtualKeyboardEnabled) {
  // Verifies that the state of the virtual keyboard accessibility feature on
  // the login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Manually enable the virtual keyboard.
  accessibility_manager->EnableVirtualKeyboard(true);
  EXPECT_TRUE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Disable the virtual keyboard through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_virtual_keyboard_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled);

  // Verify that the pref which controls the virtual keyboard in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityVirtualKeyboardEnabled));

  // Verify that the virtual keyboard cannot be enabled manually anymore.
  accessibility_manager->EnableVirtualKeyboard(true);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       VirtualKeyboardEnabledOverridesDefaultPolicy) {
  // Verifies that the state of the virtual keyboard accessibility feature on
  // the login screen will be controlled only through
  // DeviceLoginScreenVirtualKeyboardEnabled device policy if both of
  // DeviceLoginScreenVirtualKeyboardEnabled and
  // DeviceLoginScreenDefaultVirtualKeyboardEnabled have been set.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Enable the virtual keyboard through DeviceLoginScreenVirtualKeyboardEnabled
  // device policy, and disable it through
  // DeviceLoginScreenDefaultVirtualKeyboardEnabled; then wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_virtual_keyboard_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_virtual_keyboard_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_virtual_keyboard_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled);

  // Verify that the pref which controls the virtual keyboard in the login
  // profile is managed by the policy and is enabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityVirtualKeyboardEnabled));

  // Disable the virtual keyboard through
  // DeviceLoginScreenVirtualKeyboardEnabled device policy, and enable it
  // through DeviceLoginScreenDefaultVirtualKeyboardEnabled; then wait for the
  // change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_virtual_keyboard_enabled(false);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_virtual_keyboard_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_virtual_keyboard_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled);

  // Verify that the pref which controls the virtual keyboard in the login
  // profile is managed by the policy and is disabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityVirtualKeyboardEnabled));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenDictationEnabled) {
  // Verifies that the state of the dictation accessibility feature on the
  // login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());

  // Manually enable the dictation.
  PrefService* prefs = login_profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  prefs->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, true);
  EXPECT_TRUE(accessibility_manager->IsDictationEnabled());

  // Disable the dictation through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_dictation_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityDictationEnabled);

  // Verify that the pref which controls the dictation in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityDictationEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityDictationEnabled));

  // Verify that the dictation cannot be enabled manually anymore.
  prefs->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, true);
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());

  // Enable the dictation through device policy as a recommended value and wait
  // for the change to take effect.
  proto.mutable_accessibility_settings()->set_login_screen_dictation_enabled(
      true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_dictation_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityDictationEnabled);

  // Verify that the pref which controls the dictation in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityDictationEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityDictationEnabled));

  // Verify that the dictation can be enabled manually again.
  prefs->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, false);
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenSelectToSpeakEnabled) {
  // Verifies that the state of the select to speak accessibility feature on the
  // login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());

  // Manually enable the select to speak.
  accessibility_manager->SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsSelectToSpeakEnabled());

  // Disable the select to speak through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_select_to_speak_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySelectToSpeakEnabled);

  // Verify that the pref which controls the select to speak in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilitySelectToSpeakEnabled));

  // Verify that the select to speak cannot be enabled manually anymore.
  accessibility_manager->SetSelectToSpeakEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());

  // Enable the select to speak through device policy as a recommended value and
  // wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_select_to_speak_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_select_to_speak_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySelectToSpeakEnabled);

  // Verify that the pref which controls the select to speak in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilitySelectToSpeakEnabled));

  // Verify that the select to speak can be enabled manually again.
  accessibility_manager->SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenCursorHighlightEnabled) {
  // Verifies that the state of the cursor highlight accessibility feature on
  // the login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());

  // Manually enable the cursor highlight.
  accessibility_manager->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsCursorHighlightEnabled());

  // Disable the cursor highlight through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_cursor_highlight_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityCursorHighlightEnabled);

  // Verify that the pref which controls the cursor highlight in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityCursorHighlightEnabled));

  // Verify that the cursor highlight cannot be enabled manually anymore.
  accessibility_manager->SetCursorHighlightEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());

  // Enable the cursor highlight through device policy as a recommended value
  // and wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_cursor_highlight_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_cursor_highlight_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityCursorHighlightEnabled);

  // Verify that the pref which controls the cursor highlight in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityCursorHighlightEnabled));

  // Verify that the cursor highlight can be enabled manually again.
  accessibility_manager->SetCursorHighlightEnabled(false);
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenCaretHighlightEnabled) {
  // Verifies that the state of the caret highlight accessibility feature on
  // the login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());

  // Manually enable the caret highlight.
  accessibility_manager->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsCaretHighlightEnabled());

  // Disable the caret highlight through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_caret_highlight_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityCaretHighlightEnabled);

  // Verify that the pref which controls the caret highlight in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityCaretHighlightEnabled));

  // Verify that the caret highlight cannot be enabled manually anymore.
  accessibility_manager->SetCaretHighlightEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());

  // Enable the caret highlight through device policy as a recommended value and
  // wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_caret_highlight_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_caret_highlight_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityCaretHighlightEnabled);

  // Verify that the pref which controls the caret highlight in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityCaretHighlightEnabled));

  // Verify that the caret highlight can be enabled manually again.
  accessibility_manager->SetCaretHighlightEnabled(false);
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenMonoAudioEnabled) {
  // Verifies that the state of the mono audio accessibility feature on
  // the login screen can be controlled through device policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());

  // Manually enable the mono audio.
  accessibility_manager->EnableMonoAudio(true);
  EXPECT_TRUE(accessibility_manager->IsMonoAudioEnabled());

  // Disable the mono audio through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_mono_audio_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityMonoAudioEnabled);

  // Verify that the pref which controls the mono audio in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityMonoAudioEnabled));

  // Verify that the mono audio cannot be enabled manually anymore.
  accessibility_manager->EnableMonoAudio(true);
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());

  // Enable the mono audio through device policy as a recommended value and wait
  // for the change to take effect.
  proto.mutable_accessibility_settings()->set_login_screen_mono_audio_enabled(
      true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_mono_audio_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityMonoAudioEnabled);

  // Verify that the pref which controls the mono audio in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityMonoAudioEnabled));

  // Verify that the mono audio can be enabled manually again.
  accessibility_manager->EnableMonoAudio(false);
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());
}
}  // namespace policy

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {
class FakeSessionManagerClient;
}

namespace policy {

class DevicePolicyCrosTestHelper {
 public:
  DevicePolicyCrosTestHelper();
  DevicePolicyCrosTestHelper(const DevicePolicyCrosTestHelper&) = delete;
  DevicePolicyCrosTestHelper& operator=(const DevicePolicyCrosTestHelper&) =
      delete;
  ~DevicePolicyCrosTestHelper();

  DevicePolicyBuilder* device_policy() { return &device_policy_; }
  const std::string device_policy_blob();

  // Writes the owner key to disk. To be called before installing a policy.
  void InstallOwnerKey();

  // Reinstalls |device_policy_| as the policy (to be used when it was
  // recently changed).
  void RefreshDevicePolicy();
  // Refreshes the Device Settings policies given in the settings vector.
  // Example: {chromeos::kDeviceDisplayResolution} refreshes the display
  //   resolution setting.
  void RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
      const std::vector<std::string>& settings);
  void UnsetPolicy(const std::vector<std::string>& settings);

 private:
  static void OverridePaths();

  // Carries Chrome OS device policies for tests.
  DevicePolicyBuilder device_policy_;
};

// Used to test Device policy changes in Chrome OS.
class DevicePolicyCrosBrowserTest : public MixinBasedInProcessBrowserTest {
 protected:
  DevicePolicyCrosBrowserTest();
  DevicePolicyCrosBrowserTest(const DevicePolicyCrosBrowserTest&) = delete;
  DevicePolicyCrosBrowserTest& operator=(const DevicePolicyCrosBrowserTest&) =
      delete;
  ~DevicePolicyCrosBrowserTest() override;

  void RefreshDevicePolicy() { policy_helper()->RefreshDevicePolicy(); }
  chromeos::DBusThreadManagerSetter* dbus_setter() {
    return dbus_setter_.get();
  }

  DevicePolicyBuilder* device_policy() {
    return policy_helper()->device_policy();
  }

  chromeos::FakeSessionManagerClient* session_manager_client();

  chromeos::DeviceStateMixin device_state_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  DevicePolicyCrosTestHelper* policy_helper() { return &policy_helper_; }

 private:
  DevicePolicyCrosTestHelper policy_helper_;

  // FakeDBusThreadManager uses FakeSessionManagerClient.
  std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_

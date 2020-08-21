// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/hid_controller_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screens_utils.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "content/public/test/browser_test.h"

using testing::_;

namespace chromeos {

namespace {

const test::UIPath hid_continue_button = {"hid-detection",
                                          "hid-continue-button"};

}

class HidDetectionTest : public OobeBaseTest {
 public:
  HidDetectionTest() {
    // HID detection screen only appears for Chromebases, Chromebits, and
    // Chromeboxes.
    base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBOX",
                                                 base::Time::Now());
  }

  ~HidDetectionTest() override = default;

 protected:
  void ConnectDevices() {
    hid_controller_.AddUsbMouse(test::HIDControllerMixin::kMouseId);
    hid_controller_.AddUsbKeyboard(test::HIDControllerMixin::kKeyboardId);
  }

  test::HIDControllerMixin hid_controller_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(HidDetectionTest);
};

class HidDetectionSkipTest : public HidDetectionTest {
 public:
  HidDetectionSkipTest() { ConnectDevices(); }

  ~HidDetectionSkipTest() override = default;
};

IN_PROC_BROWSER_TEST_F(HidDetectionTest, NoDevicesConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(HidDetectionSkipTest, BothDevicesPreConnected) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

// Start without devices, connect them and proceed to the network screen.
// Network screen should be saved in the local state.
IN_PROC_BROWSER_TEST_F(HidDetectionTest, PRE_ResumableScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectDisabledPath(hid_continue_button);
  ConnectDevices();
  test::OobeJS().CreateEnabledWaiter(true, hid_continue_button)->Wait();
  test::OobeJS().TapOnPath(hid_continue_button);

  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::TapWelcomeNext();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Start without devices, connect them. Flow should proceed to the saved screen
// (network screen).
IN_PROC_BROWSER_TEST_F(HidDetectionTest, ResumableScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  ConnectDevices();
  test::OobeJS().CreateEnabledWaiter(true, hid_continue_button)->Wait();
  test::OobeJS().TapOnPath(hid_continue_button);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

class HidDetectionDeviceOwnedTest : public HidDetectionTest {
 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(HidDetectionDeviceOwnedTest, NoScreen) {
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
}

class HidDetectionOobeCompletedUnowned : public HidDetectionTest {
 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

IN_PROC_BROWSER_TEST_F(HidDetectionOobeCompletedUnowned, ShowScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
}

}  // namespace chromeos

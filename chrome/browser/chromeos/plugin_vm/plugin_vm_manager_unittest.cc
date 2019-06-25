// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"

#include "ash/public/cpp/shelf_model.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/fake_vm_plugin_dispatcher_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

class PluginVmManagerTest : public testing::Test {
 public:
  PluginVmManagerTest() {
    chromeos::DBusThreadManager::Initialize();
    testing_profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<PluginVmTestHelper>(testing_profile_.get());
    plugin_vm_manager_ = PluginVmManager::GetForProfile(testing_profile_.get());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  ~PluginVmManagerTest() override {
    histogram_tester_.reset();
    test_helper_.reset();
    testing_profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  chromeos::FakeVmPluginDispatcherClient& VmPluginDispatcherClient() {
    return *static_cast<chromeos::FakeVmPluginDispatcherClient*>(
        chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient());
  }
  chromeos::FakeConciergeClient& ConciergeClient() {
    return *static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }
  chromeos::FakeSeneschalClient& SeneschalClient() {
    return *static_cast<chromeos::FakeSeneschalClient*>(
        chromeos::DBusThreadManager::Get()->GetSeneschalClient());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<PluginVmTestHelper> test_helper_;
  PluginVmManager* plugin_vm_manager_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmManagerTest);
};

TEST_F(PluginVmManagerTest, LaunchPluginVmRequiresPluginVmAllowed) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));
  plugin_vm_manager_->LaunchPluginVm();
  thread_bundle_.RunUntilIdle();
  EXPECT_FALSE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_FALSE(ConciergeClient().get_vm_info_called());
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kError, 1);
}

TEST_F(PluginVmManagerTest, LaunchPluginVmStartAndShow) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  // The PluginVmManager calls StartVm when the VM is not yet running.
  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  plugin_vm_manager_->LaunchPluginVm();
  thread_bundle_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_FALSE(ConciergeClient().get_vm_info_called());
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kSuccess, 1);
}

TEST_F(PluginVmManagerTest, LaunchPluginVmShowAndStop) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  // The PluginVmManager skips calling StartVm when the VM is already running.
  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  plugin_vm_manager_->LaunchPluginVm();
  thread_bundle_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().stop_vm_called());
  EXPECT_FALSE(ConciergeClient().get_vm_info_called());
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  plugin_vm_manager_->StopPluginVm();
  thread_bundle_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().stop_vm_called());

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kSuccess, 1);
}

TEST_F(PluginVmManagerTest, OnStateChangedRunningStopped) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  vm_tools::plugin_dispatcher::VmStateChangedSignal state_changed_signal;
  state_changed_signal.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(
          testing_profile_.get()));
  state_changed_signal.set_vm_name(kPluginVmName);
  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  thread_bundle_.RunUntilIdle();
  EXPECT_TRUE(ConciergeClient().get_vm_info_called());
  EXPECT_TRUE(base::DirectoryExists(
      file_manager::util::GetMyFilesFolderForProfile(testing_profile_.get())));
  EXPECT_TRUE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 1ul);

  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  thread_bundle_.RunUntilIdle();
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);
}

TEST_F(PluginVmManagerTest, LaunchPluginVmSpinner) {
  ash::ShelfModel shelf_model;
  ChromeLauncherController chrome_launcher_controller(testing_profile_.get(),
                                                      &shelf_model);
  ShelfSpinnerController* spinner_controller =
      chrome_launcher_controller.GetShelfSpinnerController();

  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  // No spinner before doing anything
  EXPECT_FALSE(spinner_controller->HasApp(kPluginVmAppId));

  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  plugin_vm_manager_->LaunchPluginVm();
  thread_bundle_.RunUntilIdle();

  // Spinner exists for first launch.
  EXPECT_TRUE(spinner_controller->HasApp(kPluginVmAppId));
  // Under normal operation, the Plugin VM window would appear and close the
  // spinner. Since the ShowVm call doesn't actually do this, manually close
  // the spinner.
  spinner_controller->CloseSpinner(kPluginVmAppId);

  plugin_vm_manager_->LaunchPluginVm();
  thread_bundle_.RunUntilIdle();
  // A second launch shouldn't show a spinner.
  EXPECT_FALSE(spinner_controller->HasApp(kPluginVmAppId));
}

TEST_F(PluginVmManagerTest, LaunchPluginVmFromSuspending) {
  // We cannot start a vm in states like SUSPENDING, so the StartVm call is
  // delayed until an appropriate state change signal is received.
  ash::ShelfModel shelf_model;
  ChromeLauncherController chrome_launcher_controller(testing_profile_.get(),
                                                      &shelf_model);
  ShelfSpinnerController* spinner_controller =
      chrome_launcher_controller.GetShelfSpinnerController();
  test_helper_->AllowPluginVm();

  vm_tools::plugin_dispatcher::VmStateChangedSignal state_changed_signal;
  state_changed_signal.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(
          testing_profile_.get()));
  state_changed_signal.set_vm_name(kPluginVmName);
  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);

  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);
  plugin_vm_manager_->LaunchPluginVm();
  thread_bundle_.RunUntilIdle();

  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_TRUE(spinner_controller->HasApp(kPluginVmAppId));

  // The launch process continues once the operation completes.
  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  thread_bundle_.RunUntilIdle();

  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
}

}  // namespace plugin_vm

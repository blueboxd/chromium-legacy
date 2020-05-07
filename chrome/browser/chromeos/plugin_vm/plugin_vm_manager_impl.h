// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_IMPL_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_uninstaller_notification.h"
#include "chrome/browser/chromeos/vm_starting_observer.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher.pb.h"
#include "chromeos/dbus/vm_plugin_dispatcher_client.h"

class Profile;

namespace plugin_vm {

// The PluginVmManagerImpl is responsible for connecting to the D-Bus services
// to manage the Plugin Vm.

class PluginVmManagerImpl
    : public PluginVmManager,
      public chromeos::VmPluginDispatcherClient::Observer {
 public:
  using LaunchPluginVmCallback = base::OnceCallback<void(bool success)>;

  explicit PluginVmManagerImpl(Profile* profile);
  ~PluginVmManagerImpl() override;

  // TODO(juwa): Don't allow launch/stop/uninstall to run simultaneously.
  // |callback| is called either when VM tools are ready or if an error occurs.
  void LaunchPluginVm(LaunchPluginVmCallback callback) override;
  void StopPluginVm(const std::string& name, bool force) override;
  void UninstallPluginVm() override;

  uint64_t seneschal_server_handle() const override;

  // chromeos::VmPluginDispatcherClient::Observer:
  void OnVmToolsStateChanged(
      const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal)
      override;
  void OnVmStateChanged(
      const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) override;

  void UpdateVmState(
      base::OnceCallback<void(bool default_vm_exists)> success_callback,
      base::OnceClosure error_callback) override;

  vm_tools::plugin_dispatcher::VmState vm_state() const override;

  void AddVmStartingObserver(chromeos::VmStartingObserver* observer) override;
  void RemoveVmStartingObserver(
      chromeos::VmStartingObserver* observer) override;

  PluginVmUninstallerNotification* uninstaller_notification_for_testing()
      const {
    return uninstaller_notification_.get();
  }

 private:
  void OnStartDispatcher(
      base::OnceCallback<void(bool default_vm_exists)> success_callback,
      base::OnceClosure error_callback,
      bool success);
  void OnListVms(
      base::OnceCallback<void(bool default_vm_exists)> success_callback,
      base::OnceClosure error_callback,
      base::Optional<vm_tools::plugin_dispatcher::ListVmResponse> reply);

  // The flow to launch a Plugin Vm. We'll probably want to add additional
  // abstraction around starting the services in the future but this is
  // sufficient for now.
  void InstallPluginVmDlc();
  void OnInstallPluginVmDlc(const std::string& err,
                            const dlcservice::DlcModuleList& dlc_module_list);
  void OnListVmsForLaunch(bool default_vm_exists);
  void StartVm();
  void OnStartVm(
      base::Optional<vm_tools::plugin_dispatcher::StartVmResponse> reply);
  void ShowVm();
  void OnShowVm(
      base::Optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply);
  void OnGetVmInfoForSharing(
      base::Optional<vm_tools::concierge::GetVmInfoResponse> reply);
  void OnDefaultSharedDirExists(const base::FilePath& dir, bool exists);
  void UninstallSucceeded();

  // Called when LaunchPluginVm() is successful.
  void LaunchSuccessful();
  // Called when LaunchPluginVm() is unsuccessful.
  void LaunchFailed(PluginVmLaunchResult result = PluginVmLaunchResult::kError);

  // The flow to uninstall Plugin Vm.
  void OnListVmsForUninstall(bool default_vm_exists);
  void StopVmForUninstall();
  void OnStopVmForUninstall(
      base::Optional<vm_tools::plugin_dispatcher::StopVmResponse> reply);
  void DestroyDiskImage();
  void OnDestroyDiskImage(
      base::Optional<vm_tools::concierge::DestroyDiskImageResponse> response);

  // Called when UninstallPluginVm() is unsuccessful.
  void UninstallFailed();

  Profile* profile_;
  std::string owner_id_;
  uint64_t seneschal_server_handle_ = 0;

  // State of the default VM's tools, kept up-to-date by signals from the
  // dispatcher.
  vm_tools::plugin_dispatcher::VmToolsState vm_tools_state_ =
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_UNKNOWN;
  // State of the default VM, kept up-to-date by signals from the dispatcher.
  vm_tools::plugin_dispatcher::VmState vm_state_ =
      vm_tools::plugin_dispatcher::VmState::VM_STATE_UNKNOWN;

  // We can't immediately start the VM when it is in states like suspending, so
  // delay until an in progress operation finishes.
  bool pending_start_vm_ = false;

  // We can't immediately destroy the VM when it is in states like
  // suspending, so delay until an in progress operation finishes.
  bool pending_destroy_disk_image_ = false;
  // |launch_vm_callbacks_| cannot be run before the vm tools are installed, so
  // delay until the tools are installed.
  bool pending_vm_tools_installed_ = false;

  std::unique_ptr<PluginVmUninstallerNotification> uninstaller_notification_;

  base::ObserverList<chromeos::VmStartingObserver> vm_starting_observers_;
  std::vector<LaunchPluginVmCallback> launch_vm_callbacks_;

  base::WeakPtrFactory<PluginVmManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginVmManagerImpl);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_IMPL_H_

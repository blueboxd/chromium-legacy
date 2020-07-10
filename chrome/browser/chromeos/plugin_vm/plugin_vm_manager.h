// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "chromeos/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher.pb.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {
class VmStartingObserver;
}  // namespace chromeos

namespace plugin_vm {

enum class PermissionType { kCamera = 0, kMicrophone = 1 };

class PluginVmPermissionsObserver : public base::CheckedObserver {
 public:
  virtual void OnPluginVmPermissionsChanged(
      plugin_vm::PermissionType permission_type,
      bool allowed) = 0;
};

class PluginVmManager : public KeyedService {
 public:
  using LaunchPluginVmCallback = base::OnceCallback<void(bool success)>;

  ~PluginVmManager() override;

  virtual void OnPrimaryUserProfilePrepared() = 0;

  virtual void LaunchPluginVm(LaunchPluginVmCallback callback) = 0;
  virtual void RelaunchPluginVm() = 0;
  virtual void StopPluginVm(const std::string& name, bool force) = 0;
  virtual void UninstallPluginVm() = 0;

  // Seneschal server handle to use for path sharing.
  virtual uint64_t seneschal_server_handle() const = 0;

  // Starts the dispatcher, then queries it for the default Vm's state, which is
  // then used to update |vm_state_|.
  // This is used as the first step of both LaunchPluginVm and UninstallPluginVm
  // to ensure that the dispatcher is running and |vm_state_| is up to date.
  //
  // Invokes |success_callback| if the state was updated, or if there is no Vm,
  // therefore no state to updated.
  // Invokes |error_callback| if the dispatcher couldn't be started, or the
  // query was unsuccessful.
  virtual void UpdateVmState(
      base::OnceCallback<void(bool default_vm_exists)> success_callback,
      base::OnceClosure error_callback) = 0;

  // Add/remove vm starting observers.
  virtual void AddVmStartingObserver(
      chromeos::VmStartingObserver* observer) = 0;
  virtual void RemoveVmStartingObserver(
      chromeos::VmStartingObserver* observer) = 0;

  // Add/remove permissions observers
  void AddPluginVmPermissionsObserver(PluginVmPermissionsObserver* observer);
  void RemovePluginVmPermissionsObserver(PluginVmPermissionsObserver* observer);

  virtual vm_tools::plugin_dispatcher::VmState vm_state() const = 0;
  bool GetPermission(PermissionType permission_type);
  void SetPermission(PermissionType permission_type, bool value);

  // Indicates whether relaunch (suspend + start) is needed for the new
  // permissions to go into effect.
  virtual bool IsRelaunchNeededForNewPermissions() const = 0;

 protected:
  PluginVmManager();

 private:
  base::flat_map<PermissionType, bool> permissions_ = {
      {PermissionType::kCamera, false},
      {PermissionType::kMicrophone, false}};

  base::ObserverList<PluginVmPermissionsObserver>
      plugin_vm_permissions_observers_;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_

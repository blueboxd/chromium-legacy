// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"

namespace plugin_vm {

PluginVmManager::PluginVmManager() = default;

PluginVmManager::~PluginVmManager() = default;

bool PluginVmManager::GetPermission(PermissionType permission_type) {
  auto it = permissions_.find(permission_type);
  DCHECK(it != permissions_.end());
  return it->second;
}

void PluginVmManager::SetPermission(PermissionType permission_type,
                                    bool value) {
  auto it = permissions_.find(permission_type);
  DCHECK(it != permissions_.end());
  if (it->second == value) {
    return;
  }
  it->second = value;
  for (auto& observer : plugin_vm_permissions_observers_) {
    observer.OnPluginVmPermissionsChanged(permission_type, value);
  }
}
void PluginVmManager::AddPluginVmPermissionsObserver(
    PluginVmPermissionsObserver* observer) {
  plugin_vm_permissions_observers_.AddObserver(observer);
}
void PluginVmManager::RemovePluginVmPermissionsObserver(
    PluginVmPermissionsObserver* observer) {
  plugin_vm_permissions_observers_.RemoveObserver(observer);
}

}  // namespace plugin_vm

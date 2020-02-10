// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/net/always_on_vpn_manager.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "components/arc/arc_prefs.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

void SetPackageErrorCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  DVLOG(1) << "Error while setting Always-On VPN package in shill: "
           << error_name << ", " << *error_data;
}

}  // namespace

namespace arc {

AlwaysOnVpnManager::AlwaysOnVpnManager(PrefService* pref_service) {
  registrar_.Init(pref_service);
  registrar_.Add(prefs::kAlwaysOnVpnPackage,
                 base::BindRepeating(&AlwaysOnVpnManager::OnPrefChanged,
                                     base::Unretained(this)));
  registrar_.Add(prefs::kAlwaysOnVpnLockdown,
                 base::BindRepeating(&AlwaysOnVpnManager::OnPrefChanged,
                                     base::Unretained(this)));
  // update once with values before we started listening
  OnPrefChanged();
}

AlwaysOnVpnManager::~AlwaysOnVpnManager() {
  std::string package =
      registrar_.prefs()->GetString(prefs::kAlwaysOnVpnPackage);
  bool lockdown = registrar_.prefs()->GetBoolean(prefs::kAlwaysOnVpnLockdown);
  if (lockdown && !package.empty()) {
    chromeos::NetworkHandler::Get()
        ->managed_network_configuration_handler()
        ->SetManagerProperty(shill::kAlwaysOnVpnPackageProperty,
                             base::Value(std::string()), base::DoNothing(),
                             base::Bind(&SetPackageErrorCallback));
  }
  registrar_.RemoveAll();
}

void AlwaysOnVpnManager::OnPrefChanged() {
  std::string always_on_vpn_package;
  bool lockdown = registrar_.prefs()->GetBoolean(prefs::kAlwaysOnVpnLockdown);
  // Only enforce blackholing if lockdown mode is enabled
  if (lockdown) {
    always_on_vpn_package =
        registrar_.prefs()->GetString(prefs::kAlwaysOnVpnPackage);
  }
  chromeos::NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->SetManagerProperty(shill::kAlwaysOnVpnPackageProperty,
                           base::Value(always_on_vpn_package),
                           base::DoNothing(),
                           base::Bind(&SetPackageErrorCallback));
}

}  // namespace arc

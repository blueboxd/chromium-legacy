// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_settings_policy_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace policy {

SystemProxySettingsPolicyHandler::SystemProxySettingsPolicyHandler(
    chromeos::CrosSettings* cros_settings)
    : cros_settings_(cros_settings),
      system_proxy_subscription_(cros_settings_->AddSettingsObserver(
          chromeos::kSystemProxySettings,
          base::BindRepeating(&SystemProxySettingsPolicyHandler::
                                  OnSystemProxySettingsPolicyChanged,
                              base::Unretained(this)))) {
  // Fire it once so we're sure we get an invocation on startup.
  OnSystemProxySettingsPolicyChanged();
}

SystemProxySettingsPolicyHandler::~SystemProxySettingsPolicyHandler() {}

void SystemProxySettingsPolicyHandler::OnSystemProxySettingsPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::Bind(
          &SystemProxySettingsPolicyHandler::OnSystemProxySettingsPolicyChanged,
          base::Unretained(this)));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  const base::Value* proxy_settings =
      cros_settings_->GetPref(chromeos::kSystemProxySettings);

  if (!proxy_settings)
    return;

  // TODO(acostinas, chromium:1042626) Start/stop and configure system traffic
  // credentials for System-proxy daemon according to policy.
}

}  // namespace policy

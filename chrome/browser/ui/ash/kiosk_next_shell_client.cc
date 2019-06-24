// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/kiosk_next_shell_client.h"

#include <utility>

#include "apps/launcher.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/account_id/account_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"

namespace {

KioskNextShellClient* g_kiosk_next_shell_client_instance = nullptr;

}  // namespace

KioskNextShellClient::KioskNextShellClient() {
  DCHECK(!g_kiosk_next_shell_client_instance);
  g_kiosk_next_shell_client_instance = this;
}

KioskNextShellClient::~KioskNextShellClient() {
  DCHECK_EQ(this, g_kiosk_next_shell_client_instance);
  g_kiosk_next_shell_client_instance = nullptr;
  ash::KioskNextShellController::Get()->SetClientAndLaunchSession(nullptr);
}

void KioskNextShellClient::Init() {
  ash::KioskNextShellController::Get()->SetClientAndLaunchSession(this);
}

// static
KioskNextShellClient* KioskNextShellClient::Get() {
  return g_kiosk_next_shell_client_instance;
}

void KioskNextShellClient::LaunchKioskNextShell(const AccountId& account_id) {
  has_launched_ = true;
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          extension_misc::kKioskNextHomeAppId);
  DCHECK(app);
  apps::LaunchPlatformApp(profile, app,
                          extensions::AppLaunchSource::kSourceChromeInternal);
}

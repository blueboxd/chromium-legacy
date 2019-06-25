// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/kiosk_next_shell_client.h"

#include "base/logging.h"

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
  // TODO(https://crbug.com/977019): Finish cleaning up this class.
}

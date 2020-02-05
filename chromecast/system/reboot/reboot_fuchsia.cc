// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/hardware/power/statecontrol/cpp/fidl.h>

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/no_destructor.h"
#include "chromecast/public/reboot_shlib.h"

using fuchsia::hardware::power::statecontrol::Admin_Suspend_Result;
using fuchsia::hardware::power::statecontrol::AdminSyncPtr;
using fuchsia::hardware::power::statecontrol::SystemPowerState;

namespace chromecast {

// RebootShlib implementation:

AdminSyncPtr& GetAdminSyncPtr() {
  static base::NoDestructor<AdminSyncPtr> g_admin;
  return *g_admin;
}

// static
void RebootShlib::Initialize(const std::vector<std::string>& argv) {
  base::fuchsia::ComponentContextForCurrentProcess()->svc()->Connect(
      GetAdminSyncPtr().NewRequest());
}

// static
void RebootShlib::Finalize() {}

// static
bool RebootShlib::IsSupported() {
  return true;
}

// static
bool RebootShlib::IsRebootSourceSupported(
    RebootShlib::RebootSource /* reboot_source */) {
  return true;
}

// static
bool RebootShlib::RebootNow(RebootSource reboot_source) {
  Admin_Suspend_Result out_result;
  zx_status_t status =
      GetAdminSyncPtr()->Suspend(SystemPowerState::REBOOT, &out_result);
  ZX_CHECK(status == ZX_OK, status) << "Failed to suspend device";
  return !out_result.is_err();
}

// static
bool RebootShlib::IsFdrForNextRebootSupported() {
  return false;
}

// static
void RebootShlib::SetFdrForNextReboot() {}

// static
bool RebootShlib::IsOtaForNextRebootSupported() {
  return false;
}

// static
void RebootShlib::SetOtaForNextReboot() {}

}  // namespace chromecast

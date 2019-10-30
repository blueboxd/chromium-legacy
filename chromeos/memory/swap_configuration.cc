// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/swap_configuration.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"

namespace chromeos {

const base::Feature kCrOSTuneMinFilelist{"CrOSTuneMinFilelist",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kCrOSMinFilelistMb{&kCrOSTuneMinFilelist,
                                                 "CrOSMinFilelistMb", -1};
namespace {

constexpr const char kMinFilelist[] = "min_filelist";

void OnSwapParameterSet(std::string parameter,
                        base::Optional<std::string> res) {
  LOG_IF(ERROR, !res.has_value())
      << "Setting swap paramter " << parameter << " failed.";
}

void ConfigureMinFilelistIfEnabled() {
  if (!base::FeatureList::IsEnabled(kCrOSTuneMinFilelist)) {
    return;
  }

  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  CHECK(debugd_client);

  int min_mb = kCrOSMinFilelistMb.Get();
  if (min_mb < 0) {
    LOG(ERROR) << "Min Filelist MB is enabled with an invalid value: "
               << min_mb;
    return;
  }

  VLOG(1) << "Setting min filelist to " << min_mb << "MB";
  debugd_client->SetSwapParameter(
      kMinFilelist, min_mb, base::BindOnce(&OnSwapParameterSet, kMinFilelist));
}

}  // namespace

void ConfigureSwap() {
  ConfigureMinFilelistIfEnabled();
}

}  // namespace chromeos

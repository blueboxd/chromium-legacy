// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/activity_tracker.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/content_switches.h"
#include "media/media_buildflags.h"

#if defined(OS_WIN)
#include "chrome/install_static/install_util.h"
#include "components/browser_watcher/extended_crash_reporting.h"
#endif


namespace chrome {

namespace {

void SetupStunProbeTrial() {
  std::map<std::string, std::string> params;
  if (!variations::GetVariationParams("StunProbeTrial2", &params))
    return;

  // The parameter, used by StartStunFieldTrial, should have the following
  // format: "request_per_ip/interval/sharedsocket/batch_size/total_batches/
  // server1:port/server2:port/server3:port/"
  std::string cmd_param = params["request_per_ip"] + "/" + params["interval"] +
                          "/" + params["sharedsocket"] + "/" +
                          params["batch_size"] + "/" + params["total_batches"] +
                          "/" + params["server1"] + "/" + params["server2"] +
                          "/" + params["server3"] + "/" + params["server4"] +
                          "/" + params["server5"] + "/" + params["server6"];

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebRtcStunProbeTrialParameter, cmd_param);
}

#if defined(OS_WIN)

void SetupExtendedCrashReporting() {
  browser_watcher::ExtendedCrashReporting* extended_crash_reporting =
      browser_watcher::ExtendedCrashReporting::SetUpIfEnabled();

  if (!extended_crash_reporting)
    return;

  // Record product, version, channel and special build strings.
  wchar_t exe_file[MAX_PATH] = {};
  CHECK(::GetModuleFileName(nullptr, exe_file, base::size(exe_file)));

  base::string16 product_name;
  base::string16 version_number;
  base::string16 channel_name;
  base::string16 special_build;
  install_static::GetExecutableVersionDetails(
      exe_file, &product_name, &version_number, &special_build, &channel_name);

  extended_crash_reporting->SetProductStrings(product_name, version_number,
                                              channel_name, special_build);
}
#endif  // defined(OS_WIN)

}  // namespace

void SetupDesktopFieldTrials() {
  prerender::ConfigureNoStatePrefetch();
  SetupStunProbeTrial();
#if defined(OS_WIN)
  SetupExtendedCrashReporting();
#endif  // defined(OS_WIN)
}

}  // namespace chrome

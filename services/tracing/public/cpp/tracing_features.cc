// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/tracing_features.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"

namespace features {

// Enables the perfetto tracing backend. For startup tracing, pass the
// --enable-perfetto flag instead.
const base::Feature kTracingPerfettoBackend {
  "TracingPerfettoBackend",
#if defined(IS_CHROMECAST)

      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Causes the BackgroundTracingManager to upload proto messages via UMA,
// rather than JSON via the crash frontend.
const base::Feature kBackgroundTracingProtoOutput{
    "BackgroundTracingProtoOutput", base::FEATURE_DISABLED_BY_DEFAULT};

// Causes Perfetto to run in-process mode for in-process tracing producers.
const base::Feature kPerfettoForceOutOfProcessProducer{
    "PerfettoForceOutOfProcessProducer", base::FEATURE_DISABLED_BY_DEFAULT};

// Runs the tracing service as an in-process browser service.
const base::Feature kTracingServiceInProcess {
  "TracingServiceInProcess",
#if defined(OS_ANDROID) || defined(IS_CHROMECAST)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kEnablePerfettoSystemTracing{
    "EnablePerfettoSystemTracing", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace tracing {

bool TracingUsesPerfettoBackend() {
  // This is checked early at startup, so feature list may not be initialized.
  // So, for startup tracing cases there is no way to control the backend using
  // feature list.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePerfetto)) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePerfetto)) {
    return true;
  }

  if (base::FeatureList::GetInstance()) {
    return base::FeatureList::IsEnabled(features::kTracingPerfettoBackend);
  }

  return features::kTracingPerfettoBackend.default_state ==
         base::FEATURE_ENABLED_BY_DEFAULT;
}

}  // namespace tracing

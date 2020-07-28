// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_METRICS_METRICS_TEST_HELPER_H_
#define WEBLAYER_BROWSER_ANDROID_METRICS_METRICS_TEST_HELPER_H_

#include <string>

#include "base/callback.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace weblayer {
class ProfileImpl;

// Various utilities to bridge to Java code for metrics related tests.

using OnLogsMetricsCallback =
    base::RepeatingCallback<void(metrics::ChromeUserMetricsExtension)>;

// Call this in the SetUp() test harness method to install the test
// GmsBridge and to set the metrics user consent state.
void InstallTestGmsBridge(
    bool has_user_consent,
    const OnLogsMetricsCallback on_log_metrics = OnLogsMetricsCallback());

// Call this in the TearDown() test harness method to remove the GmsBridge.
void RemoveTestGmsBridge();

// See Profile::Create()'s comments for the semantics of |name|.
ProfileImpl* CreateProfile(const std::string& name);

void DestroyProfile(const std::string& name);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_METRICS_METRICS_TEST_HELPER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/lacros_metrics_provider.h"

#include "base/metrics/histogram_functions.h"

LacrosMetricsProvider::LacrosMetricsProvider() = default;

LacrosMetricsProvider::~LacrosMetricsProvider() = default;

void LacrosMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  base::UmaHistogramBoolean("ChromeOS.IsLacrosBrowser", true);
}

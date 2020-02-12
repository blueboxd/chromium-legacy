// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/histogram_util.h"

#include "base/metrics/histogram_macros.h"

namespace metrics {
namespace structured {

void LogInternalError(StructuredMetricsError error) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.InternalError", error);
}

void LogPrefReadError(PersistentPrefStore::PrefReadError error) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.PrefReadError", error,
                            PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM);
}

void LogEventRecordingState(EventRecordingState state) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.EventRecordingState", state);
}

void LogNumEventsInUpload(const int num_events) {
  UMA_HISTOGRAM_COUNTS_1000("UMA.StructuredMetrics.NumEventsInUpload",
                            num_events);
}

void LogKeyValidation(KeyValidationState state) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.KeyValidationState", state);
}

}  // namespace structured
}  // namespace metrics

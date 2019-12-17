// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include "base/message_loop/message_loop_current.h"
#include "base/values.h"
#include "components/metrics/structured/event_base.h"
#include "components/prefs/json_pref_store.h"

namespace metrics {
namespace structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;
using PrefReadError = ::PersistentPrefStore::PrefReadError;

}  // namespace

int StructuredMetricsProvider::kMaxEventsPerUpload = 100;

char StructuredMetricsProvider::kStorageFileName[] = "structured_metrics.json";

// TODO(crbug.com/1016655): Add error and usage UMA metrics.

StructuredMetricsProvider::StructuredMetricsProvider() = default;

StructuredMetricsProvider::~StructuredMetricsProvider() {
  if (storage_)
    storage_->RemoveObserver(this);
}

StructuredMetricsProvider::PrefStoreErrorDelegate::PrefStoreErrorDelegate() =
    default;

StructuredMetricsProvider::PrefStoreErrorDelegate::~PrefStoreErrorDelegate() =
    default;

void StructuredMetricsProvider::PrefStoreErrorDelegate::OnError(
    PrefReadError error) {
  // TODO(crbug.com/1016655): Add error metrics.
}

void StructuredMetricsProvider::OnRecord(const EventBase& event) {
  // Records the information in |event|, to be logged to UMA on the next call to
  // ProvideCurrentSessionData. Should only be called from the browser UI
  // sequence.
  if (!recording_enabled_ || !initialized_)
    return;

  // TODO(crbug.com/1016655): Add logic for hashing an event.
}

void StructuredMetricsProvider::OnProfileAdded(
    const base::FilePath& profile_path) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  if (initialized_)
    return;

  storage_ = new JsonPrefStore(
      profile_path.Append(StructuredMetricsProvider::kStorageFileName));
  storage_->AddObserver(this);

  // |storage_| takes ownership of the error delegate.
  storage_->ReadPrefsAsync(new PrefStoreErrorDelegate());
}

void StructuredMetricsProvider::OnInitializationCompleted(const bool success) {
  if (!success)
    return;
  DCHECK(!storage_->ReadOnly());
  initialized_ = true;
}

void StructuredMetricsProvider::OnRecordingEnabled() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  if (!recording_enabled_)
    Recorder::GetInstance()->AddObserver(this);
  recording_enabled_ = true;
}

void StructuredMetricsProvider::OnRecordingDisabled() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  if (recording_enabled_)
    Recorder::GetInstance()->RemoveObserver(this);
  recording_enabled_ = false;
  // TODO(crbug.com/1016655): Ensure cache of unsent logs is cleared. Launch
  // blocking.
}

void StructuredMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  // TODO(crbug.com/1016655): Add logic for uploading stored events.
}

}  // namespace structured
}  // namespace metrics

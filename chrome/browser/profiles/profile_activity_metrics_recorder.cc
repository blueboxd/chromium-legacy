// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_activity_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace {

ProfileActivityMetricsRecorder* g_profile_activity_metrics_recorder = nullptr;

// The maximum number of profiles that are recorded. This means that all
// profiles with bucket index greater than |kMaxProfileBucket| won't be included
// in the metrics.
constexpr int kMaxProfileBucket = 100;

// Long time of inactivity that is treated as if user starts the browser anew.
constexpr base::TimeDelta kLongTimeOfInactivity =
    base::TimeDelta::FromMinutes(30);

int GetMetricsBucketIndex(Profile* profile) {
  if (profile->IsGuestSession())
    return 0;

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    // This can happen if the profile is deleted.
    VLOG(1) << "Failed to read profile bucket index because attributes entry "
               "doesn't exist.";
    return -1;
  }
  return entry->GetMetricsBucketIndex();
}

void RecordProfileSessionDuration(Profile* profile,
                                  base::TimeDelta session_length) {
  if (!profile || session_length.InMinutes() <= 0)
    return;

  int profile_bucket = GetMetricsBucketIndex(profile);

  if (0 <= profile_bucket && profile_bucket <= kMaxProfileBucket) {
    base::Histogram::FactoryGet("Profile.SessionDuration.PerProfile", 0,
                                kMaxProfileBucket, kMaxProfileBucket + 1,
                                base::HistogramBase::kUmaTargetedHistogramFlag)
        ->AddCount(profile_bucket, session_length.InMinutes());
  }
}

void RecordBrowserActivation(Profile* profile) {
  DCHECK(profile);
  int profile_bucket = GetMetricsBucketIndex(profile);

  if (0 <= profile_bucket && profile_bucket <= kMaxProfileBucket) {
    UMA_HISTOGRAM_EXACT_LINEAR("Profile.BrowserActive.PerProfile",
                               profile_bucket, kMaxProfileBucket);
  }
}

void RecordUserAction(Profile* profile) {
  if (!profile)
    return;

  int profile_bucket = GetMetricsBucketIndex(profile);

  if (0 <= profile_bucket && profile_bucket <= kMaxProfileBucket) {
    UMA_HISTOGRAM_EXACT_LINEAR("Profile.UserAction.PerProfile", profile_bucket,
                               kMaxProfileBucket);
  }
}

void RecordProfilesState() {
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .RecordProfilesState();
}

void RecordAccountMetrics(Profile* profile) {
  DCHECK(profile);

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    // This can happen if the profile is deleted / for guest profile.
    return;
  }

  entry->RecordAccountMetrics();
}

}  // namespace

// static
void ProfileActivityMetricsRecorder::Initialize() {
  DCHECK(!g_profile_activity_metrics_recorder);
  g_profile_activity_metrics_recorder = new ProfileActivityMetricsRecorder();
}

// static
void ProfileActivityMetricsRecorder::CleanupForTesting() {
  DCHECK(g_profile_activity_metrics_recorder);
  delete g_profile_activity_metrics_recorder;
  g_profile_activity_metrics_recorder = nullptr;
}

void ProfileActivityMetricsRecorder::OnBrowserSetLastActive(Browser* browser) {
  Profile* active_profile = browser->profile()->GetOriginalProfile();

  RecordBrowserActivation(active_profile);
  RecordAccountMetrics(active_profile);

  if (last_active_profile_ != active_profile) {
    // No-op, if starting a new session (|last_active_profile_| is nullptr).
    RecordProfileSessionDuration(
        last_active_profile_, base::TimeTicks::Now() - profile_session_start_);

    last_active_profile_ = active_profile;
    profile_session_start_ = base::TimeTicks::Now();

    // Record state at startup (when last_profile_session_end_ is 0) and
    // whenever the user starts browsing after a longer time of inactivity. Do
    // it asynchronously because active_time of the just activated profile is
    // also updated from OnBrowserSetLastActive() in another BrowserListObserver
    // and we have no guarantee if this happens before or after this function
    // call.
    if (last_profile_session_end_.is_null() ||
        (profile_session_start_ - last_profile_session_end_ >
         kLongTimeOfInactivity)) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&RecordProfilesState));
    }
  }

  // This browsing session is still lasting.
  last_profile_session_end_ = base::TimeTicks::Now();
}

void ProfileActivityMetricsRecorder::OnSessionEnded(
    base::TimeDelta session_length,
    base::TimeTicks session_end) {
  // |session_length| can't be used here because it was measured across all
  // profiles.
  RecordProfileSessionDuration(last_active_profile_,
                               session_end - profile_session_start_);
  last_active_profile_ = nullptr;
  last_profile_session_end_ = base::TimeTicks::Now();
}

ProfileActivityMetricsRecorder::ProfileActivityMetricsRecorder() {
  BrowserList::AddObserver(this);
  metrics::DesktopSessionDurationTracker::Get()->AddObserver(this);
  action_callback_ = base::Bind(&ProfileActivityMetricsRecorder::OnUserAction,
                                base::Unretained(this));
  base::AddActionCallback(action_callback_);
}

ProfileActivityMetricsRecorder::~ProfileActivityMetricsRecorder() {
  BrowserList::RemoveObserver(this);
  metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
  base::RemoveActionCallback(action_callback_);
}

void ProfileActivityMetricsRecorder::OnUserAction(const std::string& action,
                                                  base::TimeTicks action_time) {
  RecordUserAction(last_active_profile_);
}

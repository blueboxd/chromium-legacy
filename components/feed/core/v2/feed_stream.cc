// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream_background.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/prefs/pref_service.h"

namespace feed {

FeedStream::FeedStream(
    RefreshTaskScheduler* refresh_task_scheduler,
    EventObserver* stream_event_observer,
    Delegate* delegate,
    PrefService* profile_prefs,
    FeedNetwork* feed_network,
    const base::Clock* clock,
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : refresh_task_scheduler_(refresh_task_scheduler),
      stream_event_observer_(stream_event_observer),
      delegate_(delegate),
      profile_prefs_(profile_prefs),
      feed_network_(feed_network),
      clock_(clock),
      tick_clock_(tick_clock),
      background_task_runner_(background_task_runner),
      background_(std::make_unique<FeedStreamBackground>()),
      task_queue_(this),
      user_classifier_(profile_prefs, clock),
      refresh_throttler_(profile_prefs, clock) {
  // TODO(harringtond): Use these members.
  (void)feed_network_;
}

void FeedStream::InitializeScheduling() {
  if (!IsArticlesListVisible()) {
    refresh_task_scheduler_->Cancel();
    return;
  }

  refresh_task_scheduler_->EnsureScheduled(
      GetUserClassTriggerThreshold(GetUserClass(), TriggerType::kFixedTimer));
}

FeedStream::~FeedStream() {
  // Delete |background_| in the background sequence.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce([](std::unique_ptr<FeedStreamBackground> background) {},
                     std::move(background_)));
}

void FeedStream::SetArticlesListVisible(bool is_visible) {
  profile_prefs_->SetBoolean(prefs::kArticlesListVisible, is_visible);
}

bool FeedStream::IsArticlesListVisible() {
  return profile_prefs_->GetBoolean(prefs::kArticlesListVisible);
}

UserClass FeedStream::GetUserClass() {
  return user_classifier_.GetUserClass();
}

base::Time FeedStream::GetLastFetchTime() {
  const base::Time fetch_time =
      profile_prefs_->GetTime(feed::prefs::kLastFetchAttemptTime);
  // Ignore impossible time values.
  if (fetch_time > clock_->Now())
    return base::Time();
  return fetch_time;
}

void FeedStream::OnTaskQueueIsIdle() {}

// TODO(harringtond): Ensure this function gets test coverage when fetching
// functionality is added.
ShouldRefreshResult FeedStream::ShouldRefresh(TriggerType trigger) {
  if (delegate_->IsOffline()) {
    return ShouldRefreshResult::kDontRefreshNetworkOffline;
  }

  if (!delegate_->IsEulaAccepted()) {
    return ShouldRefreshResult::kDontRefreshEulaNotAccepted;
  }

  if (!IsArticlesListVisible()) {
    return ShouldRefreshResult::kDontRefreshArticlesHidden;
  }

  // TODO(harringtond): |suppress_refreshes_until_| was historically used for
  // privacy purposes after clearing data to make sure sync data made it to the
  // server. I'm not sure we need this now. But also, it was documented as not
  // affecting manually triggered refreshes, but coded in a way that it does.
  // I've tried to keep the same functionality as the old feed code, but we
  // should revisit this.
  if (tick_clock_->NowTicks() < suppress_refreshes_until_) {
    return ShouldRefreshResult::kDontRefreshRefreshSuppressed;
  }

  const UserClass user_class = GetUserClass();

  if (clock_->Now() - GetLastFetchTime() <
      GetUserClassTriggerThreshold(user_class, trigger)) {
    return ShouldRefreshResult::kDontRefreshNotStale;
  }

  if (!refresh_throttler_.RequestQuota(user_class)) {
    return ShouldRefreshResult::kDontRefreshRefreshThrottled;
  }

  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.Scheduler.RefreshTrigger",
                            trigger);

  return ShouldRefreshResult::kShouldRefresh;
}

void FeedStream::OnEulaAccepted() {
  MaybeTriggerRefresh(TriggerType::kForegrounded);
}

void FeedStream::OnHistoryDeleted() {
  // Due to privacy, we should not fetch for a while (unless the user
  // explicitly asks for new suggestions) to give sync the time to propagate
  // the changes in history to the server.
  suppress_refreshes_until_ =
      tick_clock_->NowTicks() + kSuppressRefreshDuration;
  ClearAll();
}

void FeedStream::OnCacheDataCleared() {
  ClearAll();
}

void FeedStream::OnSignedIn() {
  ClearAll();
}

void FeedStream::OnSignedOut() {
  ClearAll();
}

void FeedStream::OnEnterForeground() {
  MaybeTriggerRefresh(TriggerType::kForegrounded);
}

void FeedStream::ExecuteRefreshTask() {
  if (!IsArticlesListVisible()) {
    // While the check and cancel isn't strictly necessary, a long lived session
    // could be issuing refreshes due to the background trigger while articles
    // are not visible.
    refresh_task_scheduler_->Cancel();
    return;
  }
  MaybeTriggerRefresh(TriggerType::kFixedTimer);
}

void FeedStream::ClearAll() {
  // TODO(harringtond): How should we handle in-progress tasks.
  stream_event_observer_->OnClearAll(clock_->Now() - GetLastFetchTime());

  // TODO(harringtond): This should result in clearing feed data
  // and _maybe_ triggering refresh with TriggerType::kNtpShown.
  // That work should be embedded in a task.
}

void FeedStream::MaybeTriggerRefresh(TriggerType trigger,
                                     bool clear_all_before_refresh) {
  stream_event_observer_->OnMaybeTriggerRefresh(trigger,
                                                clear_all_before_refresh);
  // TODO(harringtond): Implement refresh (with LoadStreamTask).
}

}  // namespace feed

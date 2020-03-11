// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "components/feed/core/common/enums.h"
#include "components/feed/core/common/user_classifier.h"
#include "components/feed/core/v2/master_refresh_throttler.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/offline_pages/task/task_queue.h"

class PrefService;

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace feed {
class FeedNetwork;
class RefreshTaskScheduler;
class FeedStreamBackground;

// Implements FeedStreamApi. |FeedStream| additionally exposes functionality
// needed by other classes within the Feed component.
class FeedStream : public FeedStreamApi,
                   public offline_pages::TaskQueue::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns true if Chrome's EULA has been accepted.
    virtual bool IsEulaAccepted() = 0;
    // Returns true if the device is offline.
    virtual bool IsOffline() = 0;
  };

  // An observer of stream events for testing and for tracking metrics.
  // Concrete implementation should have no observable effects on the Feed.
  class EventObserver {
   public:
    virtual void OnMaybeTriggerRefresh(TriggerType trigger,
                                       bool clear_all_before_refresh) = 0;
    virtual void OnClearAll(base::TimeDelta time_since_last_clear) = 0;
  };

  FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
             EventObserver* stream_event_observer,
             Delegate* delegate,
             PrefService* profile_prefs,
             FeedNetwork* feed_network,
             const base::Clock* clock,
             const base::TickClock* tick_clock,
             scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~FeedStream() override;

  FeedStream(const FeedStream&) = delete;
  FeedStream& operator=(const FeedStream&) = delete;

  // Initializes scheduling. This should be called at startup.
  void InitializeScheduling();

  // FeedStreamApi.
  void SetArticlesListVisible(bool is_visible) override;
  bool IsArticlesListVisible() override;

  // offline_pages::TaskQueue::Delegate.
  void OnTaskQueueIsIdle() override;

  // Event indicators. These functions are called from an external source
  // to indicate an event.

  // Called when Chrome's EULA has been accepted. This should happen when
  // Delegate::IsEulaAccepted() changes from false to true.
  void OnEulaAccepted();
  // Invoked when Chrome is foregrounded.
  void OnEnterForeground();
  // The user signed in to Chrome.
  void OnSignedIn();
  // The user signed out of Chrome.
  void OnSignedOut();
  // The user has deleted their Chrome history.
  void OnHistoryDeleted();
  // Chrome's cached data was cleared.
  void OnCacheDataCleared();
  // Invoked by RefreshTaskScheduler's scheduled task.
  void ExecuteRefreshTask();

  // State shared for the sake of implementing FeedStream. Typically these
  // functions are used by tasks.

  // Returns the computed UserClass for the active user.
  UserClass GetUserClass();

  // Returns the time of the last content fetch.
  base::Time GetLastFetchTime();

  // Provides access to |FeedStreamBackground|.
  // PostTask's to |background_callback| in the background thread. When
  // complete, executes |foreground_result_callback| with the result.
  template <typename R1, typename R2>
  bool RunInBackgroundAndReturn(
      const base::Location& from_here,
      base::OnceCallback<R1(FeedStreamBackground*)> background_callback,
      base::OnceCallback<void(R2)> foreground_result_callback) {
    return base::PostTaskAndReplyWithResult(
        background_task_runner_.get(), from_here,
        base::BindOnce(std::move(background_callback), background_.get()),
        std::move(foreground_result_callback));
  }

 private:
  void MaybeTriggerRefresh(TriggerType trigger,
                           bool clear_all_before_refresh = false);

  // Determines whether or not a fetch should be allowed.
  // If a fetch is allowed, quota is reserved with the assumption that a fetch
  // will follow shortly.
  ShouldRefreshResult ShouldRefresh(TriggerType trigger);

  void ClearAll();

  RefreshTaskScheduler* refresh_task_scheduler_;
  EventObserver* stream_event_observer_;
  Delegate* delegate_;
  PrefService* profile_prefs_;
  FeedNetwork* feed_network_;
  const base::Clock* clock_;
  const base::TickClock* tick_clock_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  // Owned, but should only be accessed with |background_task_runner_|.
  std::unique_ptr<FeedStreamBackground> background_;

  offline_pages::TaskQueue task_queue_;

  // Mutable state.
  UserClassifier user_classifier_;
  MasterRefreshThrottler refresh_throttler_;
  base::TimeTicks suppress_refreshes_until_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

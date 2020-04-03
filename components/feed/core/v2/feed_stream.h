// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "components/feed/core/common/enums.h"
#include "components/feed/core/common/user_classifier.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/request_throttler.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/offline_pages/task/task_queue.h"

class PrefService;

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace feed {
class FeedStore;
class StreamModel;
class FeedNetwork;
class RefreshTaskScheduler;
struct StreamModelUpdateRequest;

// Implements FeedStreamApi. |FeedStream| additionally exposes functionality
// needed by other classes within the Feed component.
class FeedStream : public FeedStreamApi,
                   public offline_pages::TaskQueue::Delegate,
                   public StreamModel::StoreObserver {
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
    virtual void OnLoadStream(LoadStreamStatus load_from_store_status,
                              LoadStreamStatus final_status) = 0;
    virtual void OnMaybeTriggerRefresh(TriggerType trigger,
                                       bool clear_all_before_refresh) = 0;
    virtual void OnClearAll(base::TimeDelta time_since_last_clear) = 0;
  };

  // Forwards to |feed::TranslateWireResponse()| by default. Can be overridden
  // for testing.
  class WireResponseTranslator {
   public:
    WireResponseTranslator() = default;
    ~WireResponseTranslator() = default;
    virtual std::unique_ptr<StreamModelUpdateRequest> TranslateWireResponse(
        feedwire::Response response,
        base::TimeDelta response_time,
        base::Time current_time);
  };

  FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
             EventObserver* stream_event_observer,
             Delegate* delegate,
             PrefService* profile_prefs,
             FeedNetwork* feed_network,
             FeedStore* feed_store,
             const base::Clock* clock,
             const base::TickClock* tick_clock,
             scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~FeedStream() override;

  FeedStream(const FeedStream&) = delete;
  FeedStream& operator=(const FeedStream&) = delete;

  // Initializes scheduling. This should be called at startup.
  void InitializeScheduling();

  // FeedStreamApi.

  void AttachSurface(SurfaceInterface*) override;
  void DetachSurface(SurfaceInterface*) override;
  void SetArticlesListVisible(bool is_visible) override;
  bool IsArticlesListVisible() override;
  void ExecuteOperations(
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChange(
      std::vector<feedstore::DataOperation> operations) override;
  bool CommitEphemeralChange(EphemeralChangeId id) override;
  bool RejectEphemeralChange(EphemeralChangeId id) override;

  // offline_pages::TaskQueue::Delegate.
  void OnTaskQueueIsIdle() override;

  // StreamModel::StoreObserver.
  void OnStoreChange(const StreamModel::StoreUpdate& update) override;

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

  void LoadModel(std::unique_ptr<StreamModel> model);

  FeedNetwork* GetNetwork() { return feed_network_; }
  FeedStore* GetStore() { return store_; }

  // Returns the computed UserClass for the active user.
  UserClass GetUserClass();

  // Returns the time of the last content fetch.
  base::Time GetLastFetchTime();

  // Determines if a FeedQuery request can be made. If successful,
  // returns |LoadStreamStatus::kNoStatus| and acquires throttler quota.
  // Otherwise returns the reason.
  LoadStreamStatus ShouldMakeFeedQueryRequest();

  // Loads |model|. Should be used for testing in place of typical model
  // loading from network or storage.
  void LoadModelForTesting(std::unique_ptr<StreamModel> model);
  offline_pages::TaskQueue* GetTaskQueueForTesting();
  void UnloadModelForTesting() { UnloadModel(); }

  // Returns the model if it is loaded, or null otherwise.
  StreamModel* GetModel() { return model_.get(); }

  const base::Clock* GetClock() { return clock_; }

  WireResponseTranslator* GetWireResponseTranslator() const {
    return wire_response_translator_;
  }

  void SetWireResponseTranslatorForTesting(
      WireResponseTranslator* wire_response_translator) {
    wire_response_translator_ = wire_response_translator;
  }

  void SetIdleCallbackForTesting(base::RepeatingClosure idle_callback);
  void SetUserClassifierForTesting(
      std::unique_ptr<UserClassifier> user_classifier);

 private:
  class SurfaceUpdater;
  class ModelStoreChangeMonitor;
  void MaybeTriggerRefresh(TriggerType trigger,
                           bool clear_all_before_refresh = false);
  void TriggerStreamLoad();
  void UnloadModel();

  void LoadStreamTaskComplete(LoadStreamTask::Result result);

  void ClearAll();

  // Unowned.

  RefreshTaskScheduler* refresh_task_scheduler_;
  EventObserver* stream_event_observer_;
  Delegate* delegate_;
  PrefService* profile_prefs_;
  FeedNetwork* feed_network_;
  FeedStore* store_;
  const base::Clock* clock_;
  const base::TickClock* tick_clock_;
  WireResponseTranslator* wire_response_translator_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  offline_pages::TaskQueue task_queue_;
  // Whether the model is being loaded. Used to prevent multiple simultaneous
  // attempts to load the model.
  bool model_loading_in_progress_ = false;
  std::unique_ptr<SurfaceUpdater> surface_updater_;
  // The stream model. Null if not yet loaded.
  // Internally, this should only be changed by |LoadModel()| and
  // |UnloadModel()|.
  std::unique_ptr<StreamModel> model_;

  // Set of (unowned) attached surfaces.
  base::ObserverList<SurfaceInterface> surfaces_;

  // Mutable state.
  std::unique_ptr<UserClassifier> user_classifier_;
  RequestThrottler request_throttler_;
  base::TimeTicks suppress_refreshes_until_;

  // To allow tests to wait on task queue idle.
  base::RepeatingClosure idle_callback_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

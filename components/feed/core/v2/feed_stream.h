// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/version.h"
#include "components/feed/core/common/enums.h"
#include "components/feed/core/common/user_classifier.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/request_throttler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/load_more_task.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/offline_pages/task/task_queue.h"

class PrefService;

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace feed {
class FeedNetwork;
class FeedStore;
class MetricsReporter;
class RefreshTaskScheduler;
class StreamModel;
class SurfaceUpdater;
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
    virtual DisplayMetrics GetDisplayMetrics() = 0;
    virtual std::string GetLanguageTag() = 0;
  };

  // Forwards to |feed::TranslateWireResponse()| by default. Can be overridden
  // for testing.
  class WireResponseTranslator {
   public:
    WireResponseTranslator() = default;
    ~WireResponseTranslator() = default;
    virtual RefreshResponseData TranslateWireResponse(
        feedwire::Response response,
        StreamModelUpdateRequest::Source source,
        base::Time current_time);
  };

  FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
             MetricsReporter* metrics_reporter,
             Delegate* delegate,
             PrefService* profile_prefs,
             FeedNetwork* feed_network,
             FeedStore* feed_store,
             const base::Clock* clock,
             const base::TickClock* tick_clock,
             const ChromeInfo& chrome_info);
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
  void LoadMore(base::OnceCallback<void(bool)> callback) override;
  void ExecuteOperations(
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChange(
      std::vector<feedstore::DataOperation> operations) override;
  bool CommitEphemeralChange(EphemeralChangeId id) override;
  bool RejectEphemeralChange(EphemeralChangeId id) override;
  DebugStreamData GetDebugStreamData() override;
  void ForceRefreshForDebugging() override;
  std::string DumpStateForDebugging() override;

  void ReportSliceViewed(const std::string& slice_id) override;
  void ReportNavigationStarted() override;
  void ReportNavigationDone() override;
  void ReportOpenAction(const std::string& slice_id) override;
  void ReportOpenInNewTabAction(const std::string& slice_id) override;
  void ReportOpenInNewIncognitoTabAction() override;
  void ReportSendFeedbackAction() override;
  void ReportLearnMoreAction() override;
  void ReportDownloadAction() override;
  void ReportRemoveAction() override;
  void ReportNotInterestedInAction() override;
  void ReportManageInterestsAction() override;
  void ReportContextMenuOpened() override;
  void ReportStreamScrolled(int distance_dp) override;

  // offline_pages::TaskQueue::Delegate.
  void OnTaskQueueIsIdle() override;

  // StreamModel::StoreObserver.
  void OnStoreChange(StreamModel::StoreUpdate update) override;

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

  void SetRequestSchedule(RequestSchedule schedule);

  FeedNetwork* GetNetwork() { return feed_network_; }
  FeedStore* GetStore() { return store_; }

  // Returns the time of the last content fetch.
  base::Time GetLastFetchTime();

  bool HasSurfaceAttached() const;

  // Determines if we should attempt loading the stream or refreshing at all.
  // Returns |LoadStreamStatus::kNoStatus| if loading may be attempted.
  LoadStreamStatus ShouldAttemptLoad(bool model_loading = false);

  // Determines if a FeedQuery request can be made. If successful,
  // returns |LoadStreamStatus::kNoStatus| and acquires throttler quota.
  // Otherwise returns the reason.
  LoadStreamStatus ShouldMakeFeedQueryRequest(bool is_load_more = false);

  // Unloads the model. Surfaces are not updated, and will remain frozen until a
  // model load is requested.
  void UnloadModel();

  // Triggers a stream load. The load will be aborted if |ShouldAttemptLoad()|
  // is not true.
  void TriggerStreamLoad();

  // Returns the model if it is loaded, or null otherwise.
  StreamModel* GetModel() { return model_.get(); }

  const base::Clock* GetClock() const { return clock_; }
  const base::TickClock* GetTickClock() const { return tick_clock_; }
  RequestMetadata GetRequestMetadata() const;

  WireResponseTranslator* GetWireResponseTranslator() const {
    return wire_response_translator_;
  }

  // Testing functionality.
  offline_pages::TaskQueue* GetTaskQueueForTesting();
  // Loads |model|. Should be used for testing in place of typical model
  // loading from network or storage.
  void LoadModelForTesting(std::unique_ptr<StreamModel> model);
  void SetWireResponseTranslatorForTesting(
      WireResponseTranslator* wire_response_translator) {
    wire_response_translator_ = wire_response_translator;
  }
  void SetIdleCallbackForTesting(base::RepeatingClosure idle_callback);

 private:
  class ModelStoreChangeMonitor;
  // A single function task to delete stored feed data and force a refresh.
  // To only be called from within a |Task|.
  void ForceRefreshForDebuggingTask();

  void InitialStreamLoadComplete(LoadStreamTask::Result result);
  void LoadMoreComplete(LoadMoreTask::Result result);
  void BackgroundRefreshComplete(LoadStreamTask::Result result);

  void ClearAll();

  // Unowned.

  RefreshTaskScheduler* refresh_task_scheduler_;
  MetricsReporter* metrics_reporter_;
  Delegate* delegate_;
  PrefService* profile_prefs_;
  FeedNetwork* feed_network_;
  FeedStore* store_;
  const base::Clock* clock_;
  const base::TickClock* tick_clock_;
  WireResponseTranslator* wire_response_translator_;

  ChromeInfo chrome_info_;

  offline_pages::TaskQueue task_queue_;
  // Whether the model is being loaded. Used to prevent multiple simultaneous
  // attempts to load the model.
  bool model_loading_in_progress_ = false;
  std::unique_ptr<SurfaceUpdater> surface_updater_;
  // The stream model. Null if not yet loaded.
  // Internally, this should only be changed by |LoadModel()| and
  // |UnloadModel()|.
  std::unique_ptr<StreamModel> model_;

  // Mutable state.
  RequestThrottler request_throttler_;
  base::TimeTicks suppress_refreshes_until_;
  std::vector<base::OnceCallback<void(bool)>> load_more_complete_callbacks_;

  // To allow tests to wait on task queue idle.
  base::RepeatingClosure idle_callback_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

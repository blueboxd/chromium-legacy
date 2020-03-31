// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/stream_model_update_request.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/prefs/pref_service.h"

namespace feed {

// Tracks UI changes in |StreamModel| and forwards them to |SurfaceInterface|s.
// Has the same lifetime as |StreamModel|.
class FeedStream::ModelMonitor : public StreamModel::Observer {
 public:
  using ContentRevision = ContentRevision;
  ModelMonitor(StreamModel* model,
               const base::ObserverList<SurfaceInterface>* surfaces)
      : model_(model), surfaces_(surfaces) {
    model_->SetObserver(this);
    const std::vector<ContentRevision>& content_list = model_->GetContentList();
    current_content_set_.insert(content_list.begin(), content_list.end());
  }
  ~ModelMonitor() override = default;
  ModelMonitor(const ModelMonitor&) = delete;
  ModelMonitor& operator=(const ModelMonitor&) = delete;

  // StreamModel::Observer.
  void OnUiUpdate(const StreamModel::UiUpdate& update) override {
    // TODO(harringtond): add shared states too.
    if (!update.content_list_changed)
      return;
    feedui::StreamUpdate stream_update;
    const std::vector<ContentRevision>& content_list = model_->GetContentList();
    for (ContentRevision content_revision : content_list) {
      AddSliceUpdate(content_revision,
                     current_content_set_.count(content_revision) == 0,
                     &stream_update);
    }
    for (const StreamModel::UiUpdate::SharedStateInfo& info :
         update.shared_states) {
      if (info.updated)
        AddSharedState(info.shared_state_id, &stream_update);
    }

    current_content_set_.clear();
    current_content_set_.insert(content_list.begin(), content_list.end());

    for (SurfaceInterface& surface : *surfaces_) {
      surface.StreamUpdate(stream_update);
    }
  }

  // Sends the initial stream state to a newly connected surface.
  void SurfaceAdded(SurfaceInterface* surface) {
    surface->InitialStreamState(GetUpdateForNewSurface());
  }

 private:
  static std::string ToSliceId(ContentRevision content_revision) {
    auto integer_value = content_revision.value();
    return std::string(reinterpret_cast<char*>(&integer_value),
                       sizeof(integer_value));
  }

  void AddSharedState(const std::string& shared_state_id,
                      feedui::StreamUpdate* stream_update) {
    const std::string* shared_state_data =
        model_->FindSharedStateData(shared_state_id);
    if (!shared_state_data)
      return;
    feedui::SharedState* added_shared_state =
        stream_update->add_new_shared_states();
    added_shared_state->set_id(shared_state_id);
    added_shared_state->set_xsurface_shared_state(*shared_state_data);
  }

  void AddSliceUpdate(ContentRevision content_revision,
                      bool is_content_new,
                      feedui::StreamUpdate* stream_update) {
    if (is_content_new) {
      feedui::Slice* slice =
          stream_update->add_updated_slices()->mutable_slice();
      slice->set_slice_id(ToSliceId(content_revision));
      const feedstore::Content* content = model_->FindContent(content_revision);
      DCHECK(content);
      slice->mutable_xsurface_slice()->set_xsurface_frame(content->frame());
    } else {
      stream_update->add_updated_slices()->set_slice_id(
          ToSliceId(content_revision));
    }
  }

  feedui::StreamUpdate GetUpdateForNewSurface() {
    feedui::StreamUpdate result;
    for (ContentRevision content_revision : model_->GetContentList()) {
      AddSliceUpdate(content_revision, /*is_content_new=*/true, &result);
    }
    for (std::string& id : model_->GetSharedStateIds()) {
      AddSharedState(id, &result);
    }

    return result;
  }

  // Owned by |FeedStream|.
  StreamModel* model_;
  const base::ObserverList<SurfaceInterface>* surfaces_;

  std::set<ContentRevision> current_content_set_;
};

std::unique_ptr<StreamModelUpdateRequest>
FeedStream::WireResponseTranslator::TranslateWireResponse(
    feedwire::Response response,
    base::TimeDelta response_time) {
  return ::feed::TranslateWireResponse(std::move(response), response_time);
}

FeedStream::FeedStream(
    RefreshTaskScheduler* refresh_task_scheduler,
    EventObserver* stream_event_observer,
    Delegate* delegate,
    PrefService* profile_prefs,
    FeedNetwork* feed_network,
    FeedStore* feed_store,
    const base::Clock* clock,
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : refresh_task_scheduler_(refresh_task_scheduler),
      stream_event_observer_(stream_event_observer),
      delegate_(delegate),
      profile_prefs_(profile_prefs),
      feed_network_(feed_network),
      store_(feed_store),
      clock_(clock),
      tick_clock_(tick_clock),
      background_task_runner_(background_task_runner),
      task_queue_(this),
      user_classifier_(profile_prefs, clock),
      refresh_throttler_(profile_prefs, clock) {
  // TODO(harringtond): Use these members.
  static WireResponseTranslator default_translator;
  wire_response_translator_ = &default_translator;
  (void)feed_network_;

  // Inserting this task first ensures that |store_| is initialized before
  // it is used.
  task_queue_.AddTask(std::make_unique<WaitForStoreInitializeTask>(store_));
}

void FeedStream::InitializeScheduling() {
  if (!IsArticlesListVisible()) {
    refresh_task_scheduler_->Cancel();
    return;
  }

  refresh_task_scheduler_->EnsureScheduled(
      GetUserClassTriggerThreshold(GetUserClass(), TriggerType::kFixedTimer));
}

FeedStream::~FeedStream() = default;

void FeedStream::TriggerStreamLoad() {
  if (model_monitor_ || model_loading_in_progress_)
    return;
  model_loading_in_progress_ = true;
  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      this, base::BindOnce(&FeedStream::LoadStreamTaskComplete,
                           base::Unretained(this))));
}

void FeedStream::LoadStreamTaskComplete(LoadStreamTask::Result result) {
  DVLOG(1) << "LoadStreamTaskComplete load_from_store_status="
           << result.load_from_store_status
           << " final_status=" << result.final_status;
  model_loading_in_progress_ = false;
}

void FeedStream::AttachSurface(SurfaceInterface* surface) {
  surfaces_.AddObserver(surface);
  if (model_monitor_) {
    model_monitor_->SurfaceAdded(surface);
  } else {
    TriggerStreamLoad();
  }
}

void FeedStream::DetachSurface(SurfaceInterface* surface) {
  surfaces_.RemoveObserver(surface);
}

void FeedStream::SetArticlesListVisible(bool is_visible) {
  profile_prefs_->SetBoolean(prefs::kArticlesListVisible, is_visible);
}

bool FeedStream::IsArticlesListVisible() {
  return profile_prefs_->GetBoolean(prefs::kArticlesListVisible);
}

void FeedStream::ExecuteOperations(
    std::vector<feedstore::DataOperation> operations) {
  if (!model_) {
    DLOG(ERROR) << "Calling ExecuteOperations before the model is loaded";
    return;
  }
  return model_->ExecuteOperations(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChange(
    std::vector<feedstore::DataOperation> operations) {
  if (!model_) {
    DLOG(ERROR) << "Calling CreateEphemeralChange before the model is loaded";
    return {};
  }
  return model_->CreateEphemeralChange(std::move(operations));
}

bool FeedStream::CommitEphemeralChange(EphemeralChangeId id) {
  if (!model_)
    return false;
  return model_->CommitEphemeralChange(id);
}

bool FeedStream::RejectEphemeralChange(EphemeralChangeId id) {
  if (!model_)
    return false;
  return model_->RejectEphemeralChange(id);
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

void FeedStream::LoadModelForTesting(std::unique_ptr<StreamModel> model) {
  LoadModel(std::move(model));
}
offline_pages::TaskQueue* FeedStream::GetTaskQueueForTesting() {
  return &task_queue_;
}

void FeedStream::OnTaskQueueIsIdle() {
  if (idle_callback_)
    idle_callback_.Run();
}

void FeedStream::SetIdleCallbackForTesting(
    base::RepeatingClosure idle_callback) {
  idle_callback_ = idle_callback;
}

void FeedStream::OnStoreChange(const StreamModel::StoreUpdate& update) {
  store_->WriteOperations(update.sequence_number, update.operations);
}

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

void FeedStream::LoadModel(std::unique_ptr<StreamModel> model) {
  DCHECK(!model_);
  model_ = std::move(model);
  model_monitor_ =
      std::make_unique<FeedStream::ModelMonitor>(model_.get(), &surfaces_);
  model_->SetStoreObserver(this);
  for (SurfaceInterface& surface : surfaces_) {
    model_monitor_->SurfaceAdded(&surface);
  }
}

void FeedStream::UnloadModel() {
  if (!model_)
    return;
  model_monitor_.reset();
  model_.reset();
}

}  // namespace feed

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <memory>
#include <string>
#include <utility>

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/stream_model_update_request.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

std::unique_ptr<StreamModel> LoadModelFromStore(FeedStore* store) {
  LoadStreamFromStoreTask::Result result;
  auto complete = [&](LoadStreamFromStoreTask::Result task_result) {
    result = std::move(task_result);
  };
  LoadStreamFromStoreTask load_task(
      store, /*clock=*/nullptr,
      UserClass::kActiveSuggestionsConsumer,  // Has no effect.
      base::BindLambdaForTesting(complete));
  // We want to load the data no matter how stale.
  load_task.IgnoreStalenessForTesting();

  base::RunLoop run_loop;
  load_task.Execute(run_loop.QuitClosure());
  run_loop.Run();

  if (result.status == LoadStreamStatus::kLoadedFromStore) {
    auto model = std::make_unique<StreamModel>();
    model->Update(std::move(result.update_request));
    return model;
  }
  LOG(WARNING) << "LoadModelFromStore failed with " << result.status;
  return nullptr;
}

// Returns the model state string (|StreamModel::DumpStateForTesting()|),
// given a model initialized with |update_request| and having |operations|
// applied.
std::string ModelStateFor(
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    std::vector<feedstore::DataOperation> operations = {},
    std::vector<feedstore::DataOperation> more_operations = {}) {
  StreamModel model;
  model.Update(std::move(update_request));
  model.ExecuteOperations(operations);
  model.ExecuteOperations(more_operations);
  return model.DumpStateForTesting();
}

// Returns the model state string (|StreamModel::DumpStateForTesting()|),
// given a model initialized with |store|.
std::string ModelStateFor(FeedStore* store) {
  auto model = LoadModelFromStore(store);
  if (model) {
    return model->DumpStateForTesting();
  }
  return "{Failed to load model from store}";
}

// This is EXPECT_EQ, but also dumps the string values for ease of reading.
#define EXPECT_STRINGS_EQUAL(WANT, GOT)                                       \
  {                                                                           \
    std::string want = (WANT), got = (GOT);                                   \
    EXPECT_EQ(want, got) << "Wanted:\n" << (want) << "\nBut got:\n" << (got); \
  }

class TestSurface : public FeedStream::SurfaceInterface {
 public:
  // FeedStream::SurfaceInterface.
  void StreamUpdate(const feedui::StreamUpdate& stream_update) override {
    if (!initial_state)
      initial_state = stream_update;
    update = stream_update;
    ++update_count_;
  }

  // Test functions.

  void Clear() {
    initial_state = base::nullopt;
    update = base::nullopt;
    update_count_ = 0;
  }

  // Describe what is shown on the surface in a format that can be easily
  // asserted against.
  std::string Describe() {
    if (!initial_state)
      return "empty";

    if (update->updated_slices().size() == 1 &&
        update->updated_slices()[0].has_slice() &&
        update->updated_slices()[0].slice().has_zero_state_slice()) {
      return "zero-state";
    }

    std::stringstream ss;
    ss << update->updated_slices().size() << " slices";
    // If there's more than one update, we want to know that.
    if (update_count_ > 1) {
      ss << " " << update_count_ << " updates";
    }
    return ss.str();
  }

  base::Optional<feedui::StreamUpdate> initial_state;
  base::Optional<feedui::StreamUpdate> update;

 private:
  int update_count_ = 0;
};

class TestUserClassifier : public UserClassifier {
 public:
  TestUserClassifier(PrefService* pref_service, const base::Clock* clock)
      : UserClassifier(pref_service, clock) {}
  // UserClassifier.
  UserClass GetUserClass() const override {
    return overridden_user_class_.value_or(UserClassifier::GetUserClass());
  }

  // Test use.
  void OverrideUserClass(UserClass user_class) {
    overridden_user_class_ = user_class;
  }

 private:
  base::Optional<UserClass> overridden_user_class_;
};

class TestFeedNetwork : public FeedNetwork {
 public:
  // FeedNetwork implementation.
  void SendQueryRequest(
      const feedwire::Request& request,
      base::OnceCallback<void(QueryRequestResult)> callback) override {
    ++send_query_call_count;
    // Emulate a successful response.
    // The response body is currently an empty message, because most of the
    // time we want to inject a translated response for ease of test-writing.
    query_request_sent = request;
    QueryRequestResult result;
    result.status_code = 200;
    result.response_body = std::make_unique<feedwire::Response>();
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  }
  void SendActionRequest(
      const feedwire::ActionRequest& request,
      base::OnceCallback<void(ActionRequestResult)> callback) override {
    NOTIMPLEMENTED();
  }
  void CancelRequests() override { NOTIMPLEMENTED(); }

  base::Optional<feedwire::Request> query_request_sent;
  int send_query_call_count = 0;
};

// Forwards to |FeedStream::WireResponseTranslator| unless a response is
// injected.
class TestWireResponseTranslator : public FeedStream::WireResponseTranslator {
 public:
  std::unique_ptr<StreamModelUpdateRequest> TranslateWireResponse(
      feedwire::Response response,
      base::TimeDelta response_time,
      base::Time current_time) override {
    if (injected_response_) {
      return std::move(injected_response_);
    }
    return FeedStream::WireResponseTranslator::TranslateWireResponse(
        std::move(response), response_time, current_time);
  }
  void InjectResponse(std::unique_ptr<StreamModelUpdateRequest> response) {
    injected_response_ = std::move(response);
  }
  bool InjectedResponseConsumed() const { return !injected_response_; }

 private:
  std::unique_ptr<StreamModelUpdateRequest> injected_response_;
};

class FakeRefreshTaskScheduler : public RefreshTaskScheduler {
 public:
  // RefreshTaskScheduler implementation.
  void EnsureScheduled(base::TimeDelta period) override {
    scheduled_period = period;
  }
  void Cancel() override { canceled = true; }
  void RefreshTaskComplete() override { refresh_task_complete = true; }

  base::Optional<base::TimeDelta> scheduled_period;
  bool canceled = false;
  bool refresh_task_complete = false;
};

class TestEventObserver : public FeedStream::EventObserver {
 public:
  // FeedStreamUnittest::StreamEventObserver.
  void OnLoadStream(LoadStreamStatus load_from_store_status,
                    LoadStreamStatus final_status) override {
    load_stream_status = final_status;
    LOG(INFO) << "OnLoadStream: " << final_status
              << " (store status: " << load_from_store_status << ")";
  }
  void OnMaybeTriggerRefresh(TriggerType trigger,
                             bool clear_all_before_refresh) override {
    refresh_trigger_type = trigger;
  }
  void OnClearAll(base::TimeDelta time_since_last_clear) override {
    this->time_since_last_clear = time_since_last_clear;
  }

  // Test access.

  base::Optional<LoadStreamStatus> load_stream_status;
  base::Optional<base::TimeDelta> time_since_last_clear;
  base::Optional<TriggerType> refresh_trigger_type;
};

class FeedStreamTest : public testing::Test, public FeedStream::Delegate {
 public:
  void SetUp() override {
    feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    CHECK_EQ(kTestTimeEpoch, task_environment_.GetMockClock()->Now());
    stream_ = std::make_unique<FeedStream>(
        &refresh_scheduler_, &event_observer_, this, &profile_prefs_, &network_,
        store_.get(), task_environment_.GetMockClock(),
        task_environment_.GetMockTickClock(),
        task_environment_.GetMainThreadTaskRunner());

    // Set the user classifier.
    auto user_classifier = std::make_unique<TestUserClassifier>(
        &profile_prefs_, task_environment_.GetMockClock());
    user_classifier_ = user_classifier.get();
    stream_->SetUserClassifierForTesting(std::move(user_classifier));

    WaitForIdleTaskQueue();  // Wait for any initialization.

    stream_->SetWireResponseTranslatorForTesting(&response_translator_);
  }

  void TearDown() override {
    // Ensure the task queue can return to idle. Failure to do so may be due
    // to a stuck task that never called |TaskComplete()|.
    WaitForIdleTaskQueue();
    // Store requires PostTask to clean up.
    store_.reset();
    task_environment_.RunUntilIdle();
  }

  // FeedStream::Delegate.
  bool IsEulaAccepted() override { return is_eula_accepted_; }
  bool IsOffline() override { return is_offline_; }

  // For tests.

  bool IsTaskQueueIdle() const {
    return !stream_->GetTaskQueueForTesting()->HasPendingTasks() &&
           !stream_->GetTaskQueueForTesting()->HasRunningTask();
  }

  void WaitForIdleTaskQueue() {
    if (IsTaskQueueIdle())
      return;
    base::test::ScopedRunLoopTimeout run_timeout(
        FROM_HERE, base::TimeDelta::FromSeconds(1));
    base::RunLoop run_loop;
    stream_->SetIdleCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void UnloadModel() {
    WaitForIdleTaskQueue();
    stream_->UnloadModelForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestUserClassifier* user_classifier_;
  TestEventObserver event_observer_;
  TestingPrefServiceSimple profile_prefs_;
  TestFeedNetwork network_;
  TestWireResponseTranslator response_translator_;

  std::unique_ptr<FeedStore> store_ = std::make_unique<FeedStore>(
      leveldb_proto::ProtoDatabaseProvider::GetUniqueDB<feedstore::Record>(
          leveldb_proto::ProtoDbType::FEED_STREAM_DATABASE,
          /*file_path=*/{},
          task_environment_.GetMainThreadTaskRunner()));
  FakeRefreshTaskScheduler refresh_scheduler_;
  std::unique_ptr<FeedStream> stream_;
  bool is_eula_accepted_ = true;
  bool is_offline_ = false;
};

TEST_F(FeedStreamTest, IsArticlesListVisibleByDefault) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, SetArticlesListVisible) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
  stream_->SetArticlesListVisible(false);
  EXPECT_FALSE(stream_->IsArticlesListVisible());
  stream_->SetArticlesListVisible(true);
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, RefreshIsScheduledOnInitialize) {
  stream_->InitializeScheduling();
  EXPECT_TRUE(refresh_scheduler_.scheduled_period);
}

TEST_F(FeedStreamTest, ScheduledRefreshTriggersRefresh) {
  stream_->InitializeScheduling();
  stream_->ExecuteRefreshTask();

  EXPECT_EQ(TriggerType::kFixedTimer, event_observer_.refresh_trigger_type);
  // TODO(harringtond): Once we actually perform the refresh, make sure
  // RefreshTaskComplete() is called.
  // EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
}

TEST_F(FeedStreamTest, DoNotRefreshIfArticlesListIsHidden) {
  stream_->SetArticlesListVisible(false);
  stream_->InitializeScheduling();
  stream_->ExecuteRefreshTask();

  EXPECT_TRUE(refresh_scheduler_.canceled);
  EXPECT_FALSE(event_observer_.refresh_trigger_type);
}

TEST_F(FeedStreamTest, SurfaceReceivesInitialContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface;
  stream_->AttachSurface(&surface);
  ASSERT_TRUE(surface.initial_state);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedStreamTest, SurfaceReceivesInitialContentLoadedAfterAttach) {
  TestSurface surface;
  stream_->AttachSurface(&surface);
  ASSERT_FALSE(surface.initial_state);
  {
    auto model = std::make_unique<StreamModel>();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(std::move(model));
  }

  ASSERT_EQ("2 slices", surface.Describe());
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();

  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedStreamTest, SurfaceReceivesUpdatedContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface;
  stream_->AttachSurface(&surface);
  // Remove #1, add #2.
  stream_->ExecuteOperations({
      MakeOperation(MakeRemove(MakeClusterId(1))),
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });
  ASSERT_TRUE(surface.update);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  const feedui::StreamUpdate& update = surface.update.value();

  ASSERT_EQ("2 slices 2 updates", surface.Describe());
  // First slice is just an ID that matches the old 1st slice ID.
  EXPECT_EQ(initial_state.updated_slices(0).slice().slice_id(),
            update.updated_slices(0).slice_id());
  // Second slice is a new xsurface slice.
  EXPECT_NE("", update.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:2",
            update.updated_slices(1).slice().xsurface_slice().xsurface_frame());
}

TEST_F(FeedStreamTest, SurfaceReceivesSecondUpdatedContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface;
  stream_->AttachSurface(&surface);
  // Add #2.
  stream_->ExecuteOperations({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });

  // Clear the last update and add #3.
  stream_->ExecuteOperations({
      MakeOperation(MakeCluster(3, MakeRootId())),
      MakeOperation(MakeContentNode(3, MakeClusterId(3))),
      MakeOperation(MakeContent(3)),
  });

  // The last update should have only one new piece of content.
  // This verifies the current content set is tracked properly.
  ASSERT_EQ("4 slices 3 updates", surface.Describe());

  ASSERT_EQ(4, surface.update->updated_slices().size());
  EXPECT_FALSE(surface.update->updated_slices(0).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(1).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(2).has_slice());
  EXPECT_EQ("f:3", surface.update->updated_slices(3)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedStreamTest, DetachSurface) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface;
  stream_->AttachSurface(&surface);
  EXPECT_TRUE(surface.initial_state);
  stream_->DetachSurface(&surface);
  surface.Clear();

  // Arbitrary stream change. Surface should not see the update.
  stream_->ExecuteOperations({
      MakeOperation(MakeRemove(MakeClusterId(1))),
  });
  EXPECT_FALSE(surface.update);
}

TEST_F(FeedStreamTest, LoadFromNetwork) {
  // Store is empty, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  EXPECT_EQ("2 slices", surface.Describe());
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       stream_->GetModel()->DumpStateForTesting());
  // Verify the data was written to the store.
  EXPECT_STRINGS_EQUAL(ModelStateFor(store_.get()),
                       ModelStateFor(MakeTypicalInitialModelState()));
}

TEST_F(FeedStreamTest, LoadFromNetworkBecauseStoreIsStale) {
  // Fill the store with stream data that is just barely stale, and verify we
  // fetch new data over the network.
  user_classifier_->OverrideUserClass(UserClass::kActiveSuggestionsConsumer);
  store_->SaveFullStream(MakeTypicalInitialModelState(

                             kTestTimeEpoch - base::TimeDelta::FromHours(12) -
                             base::TimeDelta::FromMinutes(1)),
                         base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
}

TEST_F(FeedStreamTest, LoadFromNetworkFailsDueToProtoTranslation) {
  // No data in the store, so we should fetch from the network.
  // The network will respond with an empty response, which should fail proto
  // translation.
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            event_observer_.load_stream_status);
}

TEST_F(FeedStreamTest, DoNotLoadFromNetworkWhenOffline) {
  is_offline_ = true;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kCannotLoadFromNetworkOffline,
            event_observer_.load_stream_status);
  EXPECT_EQ("zero-state", surface.Describe());
}

TEST_F(FeedStreamTest, DoNotLoadStreamWhenArticleListIsHidden) {
  stream_->SetArticlesListVisible(false);
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            event_observer_.load_stream_status);
  EXPECT_EQ("zero-state", surface.Describe());
}

TEST_F(FeedStreamTest, DoNotLoadStreamWhenEulaIsNotAccepted) {
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedEulaNotAccepted,
            event_observer_.load_stream_status);
  EXPECT_EQ("zero-state", surface.Describe());
}

TEST_F(FeedStreamTest, DoNotLoadFromNetworkAfterHistoryIsDeleted) {
  stream_->OnHistoryDeleted();
  task_environment_.FastForwardBy(kSuppressRefreshDuration -
                                  base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_EQ("zero-state", surface.Describe());

  EXPECT_EQ(LoadStreamStatus::kCannotLoadFromNetworkSupressedForHistoryDelete,
            event_observer_.load_stream_status);

  stream_->DetachSurface(&surface);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_EQ("2 slices 2 updates", surface.Describe());
}

TEST_F(FeedStreamTest, ShouldMakeFeedQueryRequestConsumesQuota) {
  LoadStreamStatus status = LoadStreamStatus::kNoStatus;
  for (; status == LoadStreamStatus::kNoStatus;
       status = stream_->ShouldMakeFeedQueryRequest()) {
  }

  ASSERT_EQ(LoadStreamStatus::kCannotLoadFromNetworkThrottled, status);
}

TEST_F(FeedStreamTest, LoadStreamFromStore) {
  // Fill the store with stream data that is just barely fresh, and verify it
  // loads.
  user_classifier_->OverrideUserClass(UserClass::kActiveSuggestionsConsumer);
  store_->SaveFullStream(MakeTypicalInitialModelState(
                             kTestTimeEpoch - base::TimeDelta::FromHours(12) +
                             base::TimeDelta::FromMinutes(1)),
                         base::DoNothing());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  ASSERT_EQ("2 slices", surface.Describe());
  EXPECT_FALSE(network_.query_request_sent);
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       stream_->GetModel()->DumpStateForTesting());
}

TEST_F(FeedStreamTest, DetachSurfaceWhileLoadingModel) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  stream_->DetachSurface(&surface);
  WaitForIdleTaskQueue();

  EXPECT_EQ("empty", surface.Describe());
  EXPECT_TRUE(network_.query_request_sent);
}

TEST_F(FeedStreamTest, AttachMultipleSurfacesLoadsModelOnce) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface;
  TestSurface other_surface;
  stream_->AttachSurface(&surface);
  stream_->AttachSurface(&other_surface);
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);

  // After load, another surface doesn't trigger any tasks.
  TestSurface later_surface;
  stream_->AttachSurface(&later_surface);

  EXPECT_TRUE(IsTaskQueueIdle());
}

TEST_F(FeedStreamTest, ModelChangesAreSavedToStorage) {
  store_->SaveFullStream(MakeTypicalInitialModelState(), base::DoNothing());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();
  ASSERT_TRUE(surface.initial_state);

  // Remove #1, add #2.
  const std::vector<feedstore::DataOperation> operations = {
      MakeOperation(MakeRemove(MakeClusterId(1))),
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  };
  stream_->ExecuteOperations(operations);

  WaitForIdleTaskQueue();

  // Verify changes are applied to storage.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState(), operations),
      ModelStateFor(store_.get()));

  // Unload and reload the model from the store, and verify we can still apply
  // operations correctly.
  stream_->DetachSurface(&surface);
  surface.Clear();
  UnloadModel();
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();
  ASSERT_TRUE(surface.initial_state);

  // Remove #2, add #3.
  const std::vector<feedstore::DataOperation> operations2 = {
      MakeOperation(MakeRemove(MakeClusterId(2))),
      MakeOperation(MakeCluster(3, MakeRootId())),
      MakeOperation(MakeContentNode(3, MakeClusterId(3))),
      MakeOperation(MakeContent(3)),
  };
  stream_->ExecuteOperations(operations2);

  WaitForIdleTaskQueue();
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState(), operations, operations2),
      ModelStateFor(store_.get()));
}

}  // namespace
}  // namespace feed

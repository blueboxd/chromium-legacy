// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/proto_printer.h"
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
      LoadStreamFromStoreTask::LoadType::kFullLoad, store, /*clock=*/nullptr,
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
  // Provide some helper functionality to attach/detach the surface.
  // This way we can auto-detach in the destructor.
  explicit TestSurface(FeedStream* stream = nullptr) {
    if (stream)
      Attach(stream);
  }

  ~TestSurface() override {
    if (stream_)
      Detach();
  }

  void Attach(FeedStream* stream) {
    EXPECT_FALSE(stream_);
    stream_ = stream;
    stream_->AttachSurface(this);
  }

  void Detach() {
    EXPECT_TRUE(stream_);
    stream_->DetachSurface(this);
    stream_ = nullptr;
  }

  // FeedStream::SurfaceInterface.
  void StreamUpdate(const feedui::StreamUpdate& stream_update) override {
    DVLOG(1) << "StreamUpdate: " << stream_update;
    // Some special-case treatment for the loading spinner. We don't count it
    // toward |initial_state|.
    bool is_initial_loading_spinner = IsInitialLoadSpinnerUpdate(stream_update);
    if (!initial_state && !is_initial_loading_spinner) {
      initial_state = stream_update;
    }
    update = stream_update;

    described_updates_.push_back(CurrentState());
  }

  // Test functions.

  void Clear() {
    initial_state = base::nullopt;
    update = base::nullopt;
    described_updates_.clear();
  }

  // Returns a description of the updates this surface received. Each update
  // is separated by ' -> '. Returns only the updates since the last call.
  std::string DescribeUpdates() {
    std::string result = base::JoinString(described_updates_, " -> ");
    described_updates_.clear();
    return result;
  }

  // The initial state of the stream, if it was received. This is nullopt if
  // only the loading spinner was seen.
  base::Optional<feedui::StreamUpdate> initial_state;
  // The last stream update received.
  base::Optional<feedui::StreamUpdate> update;

 private:
  std::string CurrentState() {
    if (update && IsInitialLoadSpinnerUpdate(*update))
      return "loading";

    if (!initial_state)
      return "empty";

    bool has_loading_spinner = false;
    for (int i = 0; i < update->updated_slices().size(); ++i) {
      const feedui::StreamUpdate_SliceUpdate& slice_update =
          update->updated_slices(i);
      if (slice_update.has_slice() &&
          slice_update.slice().has_zero_state_slice()) {
        CHECK(update->updated_slices().size() == 1)
            << "Zero state with other slices" << *update;
        // Returns either "no-cards" or "cant-refresh".
        return update->updated_slices()[0].slice().slice_id();
      }
      if (slice_update.has_slice() &&
          slice_update.slice().has_loading_spinner_slice()) {
        CHECK_EQ(i, update->updated_slices().size() - 1)
            << "Loading spinner in an unexpected place" << *update;
        has_loading_spinner = true;
      }
    }
    std::stringstream ss;
    if (has_loading_spinner) {
      ss << update->updated_slices().size() - 1 << " slices +spinner";
    } else {
      ss << update->updated_slices().size() << " slices";
    }
    return ss.str();
  }

  bool IsInitialLoadSpinnerUpdate(const feedui::StreamUpdate& update) {
    return update.updated_slices().size() == 1 &&
           update.updated_slices()[0].has_slice() &&
           update.updated_slices()[0].slice().has_loading_spinner_slice();
  }

  // The stream if it was attached using the constructor.
  FeedStream* stream_ = nullptr;
  std::vector<std::string> described_updates_;
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
    result.response_info.status_code = 200;
    result.response_info.fetch_duration = base::TimeDelta::FromMilliseconds(42);
    if (injected_response_) {
      result.response_body = std::make_unique<feedwire::Response>(
          std::move(injected_response_.value()));
    } else {
      result.response_body = std::make_unique<feedwire::Response>();
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  }
  void SendActionRequest(
      const feedwire::ActionRequest& request,
      base::OnceCallback<void(ActionRequestResult)> callback) override {
    NOTIMPLEMENTED();
  }
  void CancelRequests() override { NOTIMPLEMENTED(); }

  void InjectRealResponse() {
    base::FilePath response_file_path;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &response_file_path));
    response_file_path = response_file_path.AppendASCII(
        "components/test/data/feed/response.binarypb");
    std::string response_data;
    CHECK(base::ReadFileToString(response_file_path, &response_data));

    feedwire::Response response;
    CHECK(response.ParseFromString(response_data));

    injected_response_ = response;
  }

  base::Optional<feedwire::Request> query_request_sent;
  int send_query_call_count = 0;

 private:
  base::Optional<feedwire::Response> injected_response_;
};

// Forwards to |FeedStream::WireResponseTranslator| unless a response is
// injected.
class TestWireResponseTranslator : public FeedStream::WireResponseTranslator {
 public:
  RefreshResponseData TranslateWireResponse(
      feedwire::Response response,
      StreamModelUpdateRequest::Source source,
      base::Time current_time) override {
    if (injected_response_) {
      if (injected_response_->model_update_request)
        injected_response_->model_update_request->source = source;
      RefreshResponseData result = std::move(*injected_response_);
      injected_response_.reset();
      return result;
    }
    return FeedStream::WireResponseTranslator::TranslateWireResponse(
        std::move(response), source, current_time);
  }
  void InjectResponse(std::unique_ptr<StreamModelUpdateRequest> response) {
    injected_response_ = RefreshResponseData();
    injected_response_->model_update_request = std::move(response);
  }
  void InjectResponse(RefreshResponseData response_data) {
    injected_response_ = std::move(response_data);
  }
  bool InjectedResponseConsumed() const { return !injected_response_; }

 private:
  base::Optional<RefreshResponseData> injected_response_;
};

class FakeRefreshTaskScheduler : public RefreshTaskScheduler {
 public:
  // RefreshTaskScheduler implementation.
  void EnsureScheduled(base::Time run_time) override {
    scheduled_run_time = run_time;
  }
  void Cancel() override { canceled = true; }
  void RefreshTaskComplete() override { refresh_task_complete = true; }

  void Clear() {
    scheduled_run_time.reset();
    canceled = false;
    refresh_task_complete = false;
  }
  base::Optional<base::Time> scheduled_run_time;
  bool canceled = false;
  bool refresh_task_complete = false;
};

class TestMetricsReporter : public MetricsReporter {
 public:
  explicit TestMetricsReporter(const base::TickClock* clock)
      : MetricsReporter(clock) {}

  // MetricsReporter.
  void ContentSliceViewed(int index_in_stream) override {
    slice_viewed_index = index_in_stream;
    MetricsReporter::ContentSliceViewed(index_in_stream);
  }
  void OnLoadStream(LoadStreamStatus load_from_store_status,
                    LoadStreamStatus final_status) override {
    load_stream_status = final_status;
    LOG(INFO) << "OnLoadStream: " << final_status
              << " (store status: " << load_from_store_status << ")";
    MetricsReporter::OnLoadStream(load_from_store_status, final_status);
  }
  void OnLoadMore(LoadStreamStatus final_status) override {
    load_more_status = final_status;
    MetricsReporter::OnLoadMore(final_status);
  }
  void OnBackgroundRefresh(LoadStreamStatus final_status) override {
    background_refresh_status = final_status;
    MetricsReporter::OnBackgroundRefresh(final_status);
  }
  void OnClearAll(base::TimeDelta time_since_last_clear) override {
    this->time_since_last_clear = time_since_last_clear;
    MetricsReporter::OnClearAll(time_since_last_clear);
  }

  // Test access.

  base::Optional<int> slice_viewed_index;
  base::Optional<LoadStreamStatus> load_stream_status;
  base::Optional<LoadStreamStatus> load_more_status;
  base::Optional<LoadStreamStatus> background_refresh_status;
  base::Optional<base::TimeDelta> time_since_last_clear;
  base::Optional<TriggerType> refresh_trigger_type;
};

class FeedStreamTest : public testing::Test, public FeedStream::Delegate {
 public:
  void SetUp() override {
    feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    CHECK_EQ(kTestTimeEpoch, task_environment_.GetMockClock()->Now());
    ChromeInfo chrome_info;
    chrome_info.channel = version_info::Channel::STABLE;
    chrome_info.version = base::Version({99, 1, 9911, 2});
    stream_ = std::make_unique<FeedStream>(
        &refresh_scheduler_, &metrics_reporter_, this, &profile_prefs_,
        &network_, store_.get(), task_environment_.GetMockClock(),
        task_environment_.GetMockTickClock(), chrome_info);

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
  DisplayMetrics GetDisplayMetrics() override {
    DisplayMetrics result;
    result.density = 200;
    result.height_pixels = 800;
    result.width_pixels = 350;
    return result;
  }
  std::string GetLanguageTag() override { return "en-US"; }

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
    stream_->UnloadModel();
  }

  // Dumps the state of |FeedStore| to a string for debugging.
  std::string DumpStoreState() {
    base::RunLoop run_loop;
    std::unique_ptr<std::vector<feedstore::Record>> records;
    auto callback =
        [&](bool, std::unique_ptr<std::vector<feedstore::Record>> result) {
          records = std::move(result);
          run_loop.Quit();
        };
    store_->GetDatabaseForTesting()->LoadEntries(
        base::BindLambdaForTesting(callback));

    run_loop.Run();
    std::stringstream ss;
    for (const feedstore::Record& record : *records) {
      ss << record << '\n';
    }
    return ss.str();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestMetricsReporter metrics_reporter_{task_environment_.GetMockTickClock()};
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

TEST_F(FeedStreamTest, DoNotRefreshIfArticlesListIsHidden) {
  stream_->SetArticlesListVisible(false);
  stream_->InitializeScheduling();
  EXPECT_TRUE(refresh_scheduler_.canceled);

  stream_->ExecuteRefreshTask();
  EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            metrics_reporter_.background_refresh_status);
}

TEST_F(FeedStreamTest, BackgroundRefreshSuccess) {
  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  // Verify the refresh happened and that we can load a stream without the
  // network.
  ASSERT_TRUE(refresh_scheduler_.refresh_task_complete);
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_.background_refresh_status);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  EXPECT_FALSE(stream_->GetModel());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, BackgroundRefreshNotAttemptedWhenModelIsLoading) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  EXPECT_EQ(metrics_reporter_.background_refresh_status,
            LoadStreamStatus::kModelAlreadyLoaded);
}

TEST_F(FeedStreamTest, BackgroundRefreshNotAttemptedAfterModelIsLoaded) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  EXPECT_EQ(metrics_reporter_.background_refresh_status,
            LoadStreamStatus::kModelAlreadyLoaded);
}

TEST_F(FeedStreamTest, SurfaceReceivesInitialContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface(stream_.get());
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
  TestSurface surface(stream_.get());
  ASSERT_FALSE(surface.initial_state);
  {
    auto model = std::make_unique<StreamModel>();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(std::move(model));
  }

  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
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
  TestSurface surface(stream_.get());
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

  ASSERT_EQ("2 slices -> 2 slices", surface.DescribeUpdates());
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
  TestSurface surface(stream_.get());
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
  ASSERT_EQ("2 slices -> 3 slices -> 4 slices", surface.DescribeUpdates());

  ASSERT_EQ(4, surface.update->updated_slices().size());
  EXPECT_FALSE(surface.update->updated_slices(0).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(1).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(2).has_slice());
  EXPECT_EQ("f:3", surface.update->updated_slices(3)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedStreamTest, RemoveAllContentResultsInZeroState) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Remove both pieces of content.
  stream_->ExecuteOperations({
      MakeOperation(MakeRemove(MakeClusterId(0))),
      MakeOperation(MakeRemove(MakeClusterId(1))),
  });

  ASSERT_EQ("loading -> 2 slices -> no-cards", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DetachSurface) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface(stream_.get());
  EXPECT_TRUE(surface.initial_state);
  surface.Detach();
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
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       stream_->GetModel()->DumpStateForTesting());
  // Verify the data was written to the store.
  EXPECT_STRINGS_EQUAL(ModelStateFor(store_.get()),
                       ModelStateFor(MakeTypicalInitialModelState()));
}

TEST_F(FeedStreamTest, ForceRefreshForDebugging) {
  // First do a normal load via network that will fail.
  is_offline_ = true;
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Next, force a refresh that results in a successful load.
  is_offline_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ForceRefreshForDebugging();

  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> cant-refresh -> loading -> 2 slices",
            surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, RefreshScheduleFlow) {
  // Inject a typical network response, with a server-defined request schedule.
  {
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::TimeDelta::FromSeconds(12),
                                base::TimeDelta::FromSeconds(48)};
    RefreshResponseData response_data;
    response_data.model_update_request = MakeTypicalInitialModelState();
    response_data.request_schedule = schedule;

    response_translator_.InjectResponse(std::move(response_data));
  }
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Verify the first refresh was scheduled.
  EXPECT_EQ(kTestTimeEpoch + base::TimeDelta::FromSeconds(12),
            refresh_scheduler_.scheduled_run_time);

  // Simulate executing the background task.
  refresh_scheduler_.Clear();
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(12));
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  // Verify |RefreshTaskComplete()| was called and next refresh was scheduled.
  EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
  EXPECT_EQ(kTestTimeEpoch + base::TimeDelta::FromSeconds(48),
            refresh_scheduler_.scheduled_run_time);

  // Simulate executing the background task again.
  refresh_scheduler_.Clear();
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(48 - 12));
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  // Verify |RefreshTaskComplete()| was called and next refresh was scheduled.
  EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
  ASSERT_TRUE(refresh_scheduler_.scheduled_run_time);
  EXPECT_EQ(kTestTimeEpoch + base::TimeDelta::FromSeconds(48) +
                GetFeedConfig().default_background_refresh_interval,
            *refresh_scheduler_.scheduled_run_time);
}

TEST_F(FeedStreamTest, LoadFromNetworkBecauseStoreIsStale) {
  // Fill the store with stream data that is just barely stale, and verify we
  // fetch new data over the network.
  store_->OverwriteStream(
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch -
                                      GetFeedConfig().stale_content_threshold -
                                      base::TimeDelta::FromMinutes(1)),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  // The stored continutation token should be sent.
  EXPECT_EQ(
      "token-1",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
}

TEST_F(FeedStreamTest, LoadFromNetworkFailsDueToProtoTranslation) {
  // No data in the store, so we should fetch from the network.
  // The network will respond with an empty response, which should fail proto
  // translation.
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_.load_stream_status);
}

TEST_F(FeedStreamTest, DoNotLoadFromNetworkWhenOffline) {
  is_offline_ = true;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kCannotLoadFromNetworkOffline,
            metrics_reporter_.load_stream_status);
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DoNotLoadStreamWhenArticleListIsHidden) {
  stream_->SetArticlesListVisible(false);
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            metrics_reporter_.load_stream_status);
  EXPECT_EQ("no-cards", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DoNotLoadStreamWhenEulaIsNotAccepted) {
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedEulaNotAccepted,
            metrics_reporter_.load_stream_status);
  EXPECT_EQ("no-cards", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, LoadStreamAfterEulaIsAccepted) {
  // Connect a surface before the EULA is accepted.
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("no-cards", surface.DescribeUpdates());

  // Accept EULA, our surface should receive data.
  is_eula_accepted_ = true;
  stream_->OnEulaAccepted();
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DoNotLoadFromNetworkAfterHistoryIsDeleted) {
  stream_->OnHistoryDeleted();
  task_environment_.FastForwardBy(kSuppressRefreshDuration -
                                  base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> no-cards", surface.DescribeUpdates());

  EXPECT_EQ(LoadStreamStatus::kCannotLoadFromNetworkSupressedForHistoryDelete,
            metrics_reporter_.load_stream_status);

  surface.Detach();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  surface.Clear();
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
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
  store_->OverwriteStream(MakeTypicalInitialModelState(
                              /*first_cluster_id=*/0,
                              kTestTimeEpoch - base::TimeDelta::FromHours(12) +
                                  base::TimeDelta::FromMinutes(1)),
                          base::DoNothing());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_FALSE(network_.query_request_sent);
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       stream_->GetModel()->DumpStateForTesting());
}

TEST_F(FeedStreamTest, LoadingSpinnerIsSentInitially) {
  store_->OverwriteStream(MakeTypicalInitialModelState(), base::DoNothing());
  TestSurface surface(stream_.get());

  ASSERT_EQ("loading", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DetachSurfaceWhileLoadingModel) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  surface.Detach();
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading", surface.DescribeUpdates());
  EXPECT_TRUE(network_.query_request_sent);
}

TEST_F(FeedStreamTest, AttachMultipleSurfacesLoadsModelOnce) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  TestSurface other_surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  ASSERT_EQ("loading -> 2 slices", other_surface.DescribeUpdates());

  // After load, another surface doesn't trigger any tasks,
  // and immediately has content.
  TestSurface later_surface(stream_.get());

  ASSERT_EQ("2 slices", later_surface.DescribeUpdates());
  EXPECT_TRUE(IsTaskQueueIdle());
}

TEST_F(FeedStreamTest, ModelChangesAreSavedToStorage) {
  store_->OverwriteStream(MakeTypicalInitialModelState(), base::DoNothing());
  TestSurface surface(stream_.get());
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
  surface.Detach();
  surface.Clear();
  UnloadModel();
  surface.Attach(stream_.get());
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

TEST_F(FeedStreamTest, ReportSliceViewedIdentifiesCorrectIndex) {
  store_->OverwriteStream(MakeTypicalInitialModelState(), base::DoNothing());
  TestSurface surface;
  stream_->AttachSurface(&surface);
  WaitForIdleTaskQueue();

  stream_->ReportSliceViewed(
      surface.initial_state->updated_slices(1).slice().slice_id());
  EXPECT_EQ(1, metrics_reporter_.slice_viewed_index);
}

TEST_F(FeedStreamTest, LoadMoreAppendsContent) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(callback.Bind());
  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());
  EXPECT_EQ("2 slices +spinner -> 4 slices", surface.DescribeUpdates());
  // Load page 3.
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());
  EXPECT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, LoadMorePersistsData) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(stream_->GetModel()->DumpStateForTesting(),
                       ModelStateFor(store_.get()));
}

TEST_F(FeedStreamTest, LoadMorePersistAndLoadMore) {
  // Verify we can persist a LoadMore, and then do another LoadMore after
  // reloading state.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(callback.Bind());
  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());

  surface.Detach();
  UnloadModel();

  // Load page 3.
  surface.Attach(stream_.get());
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  WaitForIdleTaskQueue();
  callback.Clear();
  surface.Clear();
  stream_->LoadMore(callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());
  ASSERT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(stream_->GetModel()->DumpStateForTesting(),
                       ModelStateFor(store_.get()));
}

TEST_F(FeedStreamTest, LoadMoreSendsTokens) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ("2 slices +spinner -> 4 slices", surface.DescribeUpdates());

  EXPECT_EQ(
      "token-1",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_EQ("page-2", network_.query_request_sent->feed_request()
                          .feed_query()
                          .next_page_token()
                          .next_page_token()
                          .next_page_token());

  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());

  EXPECT_EQ(
      "token-2",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_EQ("page-3", network_.query_request_sent->feed_request()
                          .feed_query()
                          .next_page_token()
                          .next_page_token()
                          .next_page_token());
}

TEST_F(FeedStreamTest, LoadMoreFail) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Don't inject another response, which results in a proto translation
  // failure.
  CallbackReceiver<bool> callback;
  stream_->LoadMore(callback.Bind());
  WaitForIdleTaskQueue();

  EXPECT_EQ(base::Optional<bool>(false), callback.GetResult());
  EXPECT_EQ("2 slices +spinner -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, LoadMoreWithClearAllInResponse) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Use a different initial state (which includes a CLEAR_ALL).
  response_translator_.InjectResponse(MakeTypicalInitialModelState(5));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(stream_->GetModel()->DumpStateForTesting(),
                       ModelStateFor(store_.get()));

  // Verify the new state has been pushed to |surface|.
  ASSERT_EQ("2 slices +spinner -> 2 slices", surface.DescribeUpdates());

  const feedui::StreamUpdate& initial_state = surface.update.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:5", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:6", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedStreamTest, LoadMoreBeforeLoad) {
  CallbackReceiver<bool> callback;
  stream_->LoadMore(callback.Bind());

  EXPECT_EQ(base::Optional<bool>(false), callback.GetResult());
}

TEST_F(FeedStreamTest, ReadNetworkResponse) {
  network_.InjectRealResponse();
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 10 slices", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, ClearAllAfterLoadResultsInRefresh) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->OnCacheDataCleared();  // triggers ClearAll().

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices -> loading -> 2 slices",
            surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, ClearAllWithNoSurfacesAttachedDoesNotReload) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.Detach();

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  // Also check that the storage is cleared.
  EXPECT_EQ("", DumpStoreState());
}

}  // namespace
}  // namespace feed

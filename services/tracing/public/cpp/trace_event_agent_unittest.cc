// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_event_agent.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/mojom/tracing.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

namespace {
constexpr const char kTestCategory[] = "TraceEventAgentTestCategory";
const char kTestMetadataKey[] = "TraceEventAgentTestMetadata";
}  // namespace

class MockRecorder : public mojom::Recorder {
 public:
  explicit MockRecorder(mojo::PendingReceiver<mojom::Recorder> receiver)
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindRepeating(
        &MockRecorder::OnConnectionError, base::Unretained(this)));
  }

  std::string events() const { return events_; }
  std::string metadata() const { return metadata_; }
  void set_quit_closure(base::Closure quit_closure) {
    quit_closure_ = quit_closure;
  }

 private:
  void Append(std::string* dest, const std::string& chunk) {
    if (chunk.empty())
      return;
    if (!dest->empty())
      dest->push_back(',');
    dest->append(chunk);
  }

  void AddChunk(const std::string& chunk) override {
    std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer(
        trace_analyzer::TraceAnalyzer::Create("[" + chunk + "]"));
    trace_analyzer::TraceEventVector events;
    analyzer->FindEvents(trace_analyzer::Query::EventCategoryIs(kTestCategory),
                         &events);
    for (const auto* event : events) {
      Append(&events_, event->name);
    }
  }

  void AddMetadata(base::Value metadata) override {
    base::DictionaryValue* dict = nullptr;
    EXPECT_TRUE(metadata.GetAsDictionary(&dict));
    std::string value;
    if (dict->GetString(kTestMetadataKey, &value))
      Append(&metadata_, value);
  }

  void OnConnectionError() {
    if (quit_closure_)
      quit_closure_.Run();
  }

  mojo::Receiver<mojom::Recorder> receiver_;
  std::string events_;
  std::string metadata_;
  base::Closure quit_closure_;
};

class TraceEventAgentTest : public testing::Test {
 public:
  void SetUp() override { PerfettoTracedProcess::ResetTaskRunnerForTesting(); }

  void TearDown() override {
    base::trace_event::TraceLog::GetInstance()->SetDisabled();
    recorder_.reset();
  }

  void StartTracing(const std::string& categories) {
    TraceEventAgent::GetInstance()->StartTracing(
        base::trace_event::TraceConfig(categories, "").ToString(),
        base::TimeTicks::Now(),
        base::BindRepeating([](bool success) { EXPECT_TRUE(success); }));
  }

  void StopAndFlush(base::Closure quit_closure) {
    mojo::PendingRemote<mojom::Recorder> recorder;
    recorder_.reset(
        new MockRecorder(recorder.InitWithNewPipeAndPassReceiver()));
    recorder_->set_quit_closure(quit_closure);
    TraceEventAgent::GetInstance()->StopAndFlush(std::move(recorder));
  }

  void AddMetadataGeneratorFunction(
      TraceEventAgent::MetadataGeneratorFunction generator) {
    TraceEventAgent::GetInstance()->AddMetadataGeneratorFunction(generator);
  }

  MockRecorder* recorder() const { return recorder_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockRecorder> recorder_;
};

TEST_F(TraceEventAgentTest, StartTracing) {
  EXPECT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());
  base::RunLoop run_loop;
  StartTracing("*");
  EXPECT_TRUE(base::trace_event::TraceLog::GetInstance()->IsEnabled());
  StopAndFlush(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(TraceEventAgentTest, StopAndFlushEvents) {
  EXPECT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());
  base::RunLoop run_loop;
  StartTracing(kTestCategory);
  TRACE_EVENT_INSTANT0(kTestCategory, "event1", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0(kTestCategory, "event2", TRACE_EVENT_SCOPE_THREAD);
  StopAndFlush(run_loop.QuitClosure());
  run_loop.Run();

  auto* mock_recorder = recorder();
  EXPECT_EQ("event1,event2", mock_recorder->events());
  EXPECT_EQ("", mock_recorder->metadata());
  EXPECT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());
}

TEST_F(TraceEventAgentTest, StopAndFlushMetadata) {
  EXPECT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());
  base::RunLoop run_loop;
  AddMetadataGeneratorFunction(base::BindRepeating([] {
    std::unique_ptr<base::DictionaryValue> metadata_dict(
        new base::DictionaryValue());
    metadata_dict->SetString(kTestMetadataKey, "test metadata");
    return metadata_dict;
  }));
  StartTracing(kTestCategory);
  StopAndFlush(run_loop.QuitClosure());
  run_loop.Run();

  auto* mock_recorder = recorder();
  EXPECT_EQ("", mock_recorder->events());
  EXPECT_EQ("test metadata", mock_recorder->metadata());
  EXPECT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());
}
}  // namespace tracing

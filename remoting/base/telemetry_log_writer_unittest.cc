// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/telemetry_log_writer.h"

#include <array>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
//#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "net/http/http_status_code.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/grpc_support/grpc_async_executor.h"
#include "remoting/base/grpc_test_support/fake_client_async_response_reader.h"
#include "remoting/proto/remoting/v1/telemetry_service.grpc.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class MockTelemetryStub
    : public apis::v1::RemotingTelemetryService::StubInterface {
 public:
  ~MockTelemetryStub() override {
    // Despite being passed around in a unique_ptr, instances of
    // ClientAsyncResponseReaderInterface are expected to manage their own
    // lifetimes. (default_deleter is specialized to be a noop for such
    // instances.) Since only a small number of instances are created for each
    // test, the easiest solution is to keep track of the created instances and
    // delete them all during tear-down.
    for (auto& deleter : deleters_) {
      std::move(deleter).Run();
    }
  }
  MOCK_METHOD(grpc::Status,
              CreateEvent,
              (grpc::ClientContext * context,
               const apis::v1::CreateEventRequest& request,
               apis::v1::CreateEventResponse* response),
              (override));
  MOCK_METHOD(grpc::Status,
              CreateLogEntry,
              (grpc::ClientContext * context,
               const apis::v1::CreateLogEntryRequest& request,
               apis::v1::CreateLogEntryResponse* response),
              (override));

 private:
  grpc::ClientAsyncResponseReaderInterface<apis::v1::CreateEventResponse>*
  AsyncCreateEventRaw(grpc::ClientContext* context,
                      const apis::v1::CreateEventRequest& request,
                      grpc::CompletionQueue* cq) override {
    return RegisterDeletable(
        new test::FakeClientAsyncResponseReader<apis::v1::CreateEventResponse>(
            base::BindOnce(&MockTelemetryStub::CreateEvent,
                           base::Unretained(this), context, request),
            cq, true));
  }
  grpc::ClientAsyncResponseReaderInterface<apis::v1::CreateEventResponse>*
  PrepareAsyncCreateEventRaw(grpc::ClientContext* context,
                             const apis::v1::CreateEventRequest& request,
                             grpc::CompletionQueue* cq) override {
    return RegisterDeletable(
        new test::FakeClientAsyncResponseReader<apis::v1::CreateEventResponse>(
            base::BindOnce(&MockTelemetryStub::CreateEvent,
                           base::Unretained(this), context, request),
            cq, false));
  }
  grpc::ClientAsyncResponseReaderInterface<apis::v1::CreateLogEntryResponse>*
  AsyncCreateLogEntryRaw(grpc::ClientContext* context,
                         const apis::v1::CreateLogEntryRequest& request,
                         grpc::CompletionQueue* cq) override {
    return RegisterDeletable(new test::FakeClientAsyncResponseReader<
                             apis::v1::CreateLogEntryResponse>(
        base::BindOnce(&MockTelemetryStub::CreateLogEntry,
                       base::Unretained(this), context, request),
        cq, true));
  }
  grpc::ClientAsyncResponseReaderInterface<apis::v1::CreateLogEntryResponse>*
  PrepareAsyncCreateLogEntryRaw(grpc::ClientContext* context,
                                const apis::v1::CreateLogEntryRequest& request,
                                grpc::CompletionQueue* cq) override {
    return RegisterDeletable(new test::FakeClientAsyncResponseReader<
                             apis::v1::CreateLogEntryResponse>(
        base::BindOnce(&MockTelemetryStub::CreateLogEntry,
                       base::Unretained(this), context, request),
        cq, false));
  }

  template <typename T>
  T* RegisterDeletable(T* deletable) {
    deleters_.push_back(base::BindOnce([](T* ptr) { delete ptr; }, deletable));
    return deletable;
  }

  std::vector<base::OnceClosure> deleters_;
};

MATCHER_P(HasDurations, durations, "") {
  if (!arg.has_payload() ||
      static_cast<std::size_t>(arg.payload().events_size()) !=
          durations.size()) {
    return false;
  }
  for (std::size_t i = 0; i < durations.size(); ++i) {
    auto event = arg.payload().events(i);
    if (!event.has_session_duration() ||
        event.session_duration() != durations[i]) {
      return false;
    }
  }
  return true;
}

template <typename... Args>
std::array<int, sizeof...(Args)> MakeIntArray(Args&&... args) {
  return {std::forward<Args>(args)...};
}

// Sets expectation for call to CreateEvent with the set of events specified,
// identified by their session_duration field. (Session duration is incremented
// after each call to LogFakeEvent.)
//
// stub: The MockTelemetryStub on which to set the expectation.
// durations: The durations of the expected events, grouped with parentheses.
//     E.g., (0) or (1, 2).
//
// Example usage:
//     EXPECT_EVENTS(*mock_stub_ptr_, (1, 2)).WillOnce(kSucceed);
#define EXPECT_EVENTS(stub, durations)                                      \
  EXPECT_CALL((stub),                                                       \
              CreateEvent(testing::_, HasDurations(MakeIntArray durations), \
                          testing::NotNull()))

// Constant success and failure actions to be passed to WillOnce and friends.
const auto kSucceed =
    testing::DoAll(testing::SetArgPointee<2>(apis::v1::CreateEventResponse()),
                   testing::Return(grpc::Status()));

const auto kFail = testing::Return(
    grpc::Status(grpc::UNAVAILABLE, "The service is unavailable."));

}  // namespace

class TelemetryLogWriterTest : public testing::Test {
 public:
  TelemetryLogWriterTest()
      : mock_stub_ptr_(new testing::StrictMock<MockTelemetryStub>()),
        log_writer_(
            std::make_unique<FakeOAuthTokenGetter>(OAuthTokenGetter::SUCCESS,
                                                   "dummy",
                                                   "dummy"),
            base::WrapUnique(mock_stub_ptr_)) {}

  ~TelemetryLogWriterTest() override {
    // It's an async process to create request to send all pending events.
    RunUntilIdle();
  }

 protected:
  void LogFakeEvent() {
    ChromotingEvent entry;
    entry.SetInteger(ChromotingEvent::kSessionDurationKey, duration_);
    duration_++;
    log_writer_.Log(entry);
  }

  // Waits until TelemetryLog is idle.
  void RunUntilIdle() {
    // gRPC has its own event loop, which means sometimes the task queue will
    // be empty while gRPC is working. Thus, TaskEnvironment::RunUntilIdle can't
    // be used, as it would return early. Instead, TelemetryLogWriter is polled
    // to determine when it has finished.
    base::RunLoop run_loop;
    base::RepeatingTimer timer;
    // Mock clock will auto-fast-forward, so the delay here is somewhat
    // arbitrary.
    timer.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(1),
        base::BindRepeating(
            [](TelemetryLogWriter* log_writer,
               base::RepeatingClosure quit_closure) {
              if (log_writer->IsIdleForTesting()) {
                quit_closure.Run();
              }
            },
            base::Unretained(&log_writer_), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  MockTelemetryStub* mock_stub_ptr_;
  TelemetryLogWriter log_writer_;

 private:
  // Incremented for each event to allow them to be distinguished.
  int duration_ = 0;
  // MOCK_TIME will fast forward through back-off delays.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TelemetryLogWriterTest, PostOneLogImmediately) {
  EXPECT_EVENTS(*mock_stub_ptr_, (0)).WillOnce(kSucceed);
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostOneLogAndHaveTwoPendingLogs) {
  // First one is sent right away. Second two are batched and sent once the
  // first request has completed.
  EXPECT_EVENTS(*mock_stub_ptr_, (0)).WillOnce(kSucceed);
  EXPECT_EVENTS(*mock_stub_ptr_, (1, 2)).WillOnce(kSucceed);
  LogFakeEvent();
  LogFakeEvent();
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostLogFailedAndRetry) {
  EXPECT_EVENTS(*mock_stub_ptr_, (0)).Times(5).WillRepeatedly(kFail);
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostOneLogFailedResendWithTwoPendingLogs) {
  EXPECT_EVENTS(*mock_stub_ptr_, (0)).WillOnce(kFail);
  EXPECT_EVENTS(*mock_stub_ptr_, (0, 1, 2)).WillOnce(kSucceed);
  LogFakeEvent();
  LogFakeEvent();
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostThreeLogsFailedAndResendWithOnePending) {
  // This tests the ordering of the resent log.
  EXPECT_EVENTS(*mock_stub_ptr_, (0)).WillOnce(kFail);
  EXPECT_EVENTS(*mock_stub_ptr_, (0, 1, 2))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([this]() { LogFakeEvent(); }), kFail));
  EXPECT_EVENTS(*mock_stub_ptr_, (0, 1, 2, 3)).WillOnce(kSucceed);
  LogFakeEvent();
  LogFakeEvent();
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostOneFailedThenSucceed) {
  EXPECT_EVENTS(*mock_stub_ptr_, (0)).WillOnce(kFail).WillOnce(kSucceed);
  LogFakeEvent();
}

}  // namespace remoting

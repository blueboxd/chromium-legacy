// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pair;
using ::testing::SizeIs;

// Test observer which stores all events received.
class TestObserver : public EventHandler::Observer {
 public:
  void OnEvent(const EventHandler::EventKey& key,
               const ValueProto& value) override {
    received_events_.emplace_back(std::make_pair(key, value));
    if (callback_) {
      std::move(callback_).Run();
    }
  }

  // Sets |callback| to execute the next time an event is received.
  void RegisterOneTimeCallback(base::OnceCallback<void()> callback) {
    callback_ = std::move(callback);
  }

  // Returns the vector of all received events.
  const std::vector<std::pair<EventHandler::EventKey, ValueProto>>&
  GetEvents() {
    return received_events_;
  }

 private:
  std::vector<std::pair<EventHandler::EventKey, ValueProto>> received_events_;
  base::OnceCallback<void()> callback_;
};

TEST(EventHandlerTest, SmokeTest) {
  EventHandler handler;
  TestObserver receiver;

  handler.AddObserver(&receiver);
  handler.DispatchEvent({EventProto::kOnValueChanged, "Test"}, ValueProto());
}

TEST(EventHandlerTest, UnregisterSelfDuringNotification) {
  EventHandler handler;
  TestObserver receiver1;
  TestObserver receiver2;

  handler.AddObserver(&receiver1);
  handler.AddObserver(&receiver2);

  receiver1.RegisterOneTimeCallback(base::BindOnce(
      &EventHandler::RemoveObserver, base::Unretained(&handler), &receiver1));
  handler.DispatchEvent({EventProto::kOnValueChanged, "Test"}, ValueProto());

  EXPECT_THAT(receiver1.GetEvents(), SizeIs(1));
  EXPECT_THAT(receiver2.GetEvents(), SizeIs(1));
}

TEST(EventHandlerTest, UnregisterNextDuringNotification) {
  EventHandler handler;
  TestObserver receiver1;
  TestObserver receiver2;

  handler.AddObserver(&receiver1);
  handler.AddObserver(&receiver2);

  receiver1.RegisterOneTimeCallback(base::BindOnce(
      &EventHandler::RemoveObserver, base::Unretained(&handler), &receiver2));
  handler.DispatchEvent({EventProto::kOnValueChanged, "Test"}, ValueProto());

  EXPECT_THAT(receiver1.GetEvents(), SizeIs(1));
  EXPECT_THAT(receiver2.GetEvents(), SizeIs(0));
}

TEST(EventHandlerTest, UnregisterPreviousDuringNotification) {
  EventHandler handler;
  TestObserver receiver1;
  TestObserver receiver2;

  handler.AddObserver(&receiver1);
  handler.AddObserver(&receiver2);

  receiver2.RegisterOneTimeCallback(base::BindOnce(
      &EventHandler::RemoveObserver, base::Unretained(&handler), &receiver1));
  handler.DispatchEvent({EventProto::kOnValueChanged, "Test"}, ValueProto());
  handler.DispatchEvent({EventProto::kOnValueChanged, "Test"}, ValueProto());

  EXPECT_THAT(receiver1.GetEvents(), SizeIs(1));
  EXPECT_THAT(receiver2.GetEvents(), SizeIs(2));
}

TEST(EventHandlerTest, FireEventDuringNotification) {
  EventHandler handler;
  TestObserver receiver;
  handler.AddObserver(&receiver);

  ValueProto test;
  test.mutable_strings()->add_values("Some value");
  receiver.RegisterOneTimeCallback(base::BindOnce(
      &EventHandler::DispatchEvent, base::Unretained(&handler),
      std::make_pair(EventProto::kOnValueChanged, "Event 2"), test));
  handler.DispatchEvent({EventProto::kOnValueChanged, "Event 1"}, ValueProto());

  EXPECT_THAT(
      receiver.GetEvents(),
      ElementsAre(
          Pair(std::make_pair(EventProto::kOnValueChanged, "Event 1"),
               ValueProto()),
          Pair(std::make_pair(EventProto::kOnValueChanged, "Event 2"), test)));
}

}  // namespace
}  // namespace autofill_assistant

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/test/fake_clock.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::Invoke;
using StoreEntries = std::vector<std::unique_ptr<notifications::ClientState>>;

namespace notifications {
namespace {

const char kGuid1[] = "guid1";
const char kTimeStr[] = "04/25/20 01:00:00 AM";

struct TestCase {
  // Input data that will be pushed to the target class.
  std::vector<test::ImpressionTestData> input;

  // List of registered clients.
  std::vector<SchedulerClientType> registered_clients;

  // Expected output data.
  std::vector<test::ImpressionTestData> expected;
};

Impression CreateImpression(const base::Time& create_time,
                            const std::string& guid) {
  Impression impression(SchedulerClientType::kTest1, guid, create_time);
  impression.task_start_time = SchedulerTaskTime::kMorning;
  return impression;
}

TestCase CreateDefaultTestCase() {
  TestCase test_case;
  test_case.input = {{SchedulerClientType::kTest1,
                      2 /* current_max_daily_show */,
                      {},
                      base::nullopt /* suppression_info */}};
  test_case.registered_clients = {SchedulerClientType::kTest1};
  test_case.expected = test_case.input;
  return test_case;
}

class MockImpressionStore : public CollectionStore<ClientState> {
 public:
  MockImpressionStore() {}

  MOCK_METHOD1(InitAndLoad, void(CollectionStore<ClientState>::LoadCallback));
  MOCK_METHOD3(Add,
               void(const std::string&,
                    const ClientState&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD3(Update,
               void(const std::string&,
                    const ClientState&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD2(Delete,
               void(const std::string&, base::OnceCallback<void(bool)>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockImpressionStore);
};

class MockDelegate : public ImpressionHistoryTracker::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() = default;
  MOCK_METHOD0(OnImpressionUpdated, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

// TODO(xingliu): Add more test cases following the test doc.
class ImpressionHistoryTrackerTest : public ::testing::Test {
 public:
  ImpressionHistoryTrackerTest() : store_(nullptr), delegate_(nullptr) {}
  ~ImpressionHistoryTrackerTest() override = default;

  void SetUp() override {
    config_.impression_expiration = base::TimeDelta::FromDays(28);
    config_.suppression_duration = base::TimeDelta::FromDays(56);
    config_.initial_daily_shown_per_type = 2;
  }

 protected:
  // Creates the impression tracker.
  void CreateTracker(const TestCase& test_case) {
    auto store = std::make_unique<MockImpressionStore>();
    store_ = store.get();
    delegate_ = std::make_unique<MockDelegate>();

    impression_trakcer_ = std::make_unique<ImpressionHistoryTrackerImpl>(
        config_, test_case.registered_clients, std::move(store), &clock_);
  }

  // Initializes the tracker with data defined in the |test_case|.
  void InitTrackerWithData(const TestCase& test_case) {
    // Initialize the store and call the callback.A
    StoreEntries entries;
    test::AddImpressionTestData(test_case.input, &entries);
    EXPECT_CALL(*store_, InitAndLoad(_))
        .WillOnce(
            Invoke([&entries](base::OnceCallback<void(bool, StoreEntries)> cb) {
              std::move(cb).Run(true, std::move(entries));
            }));
    base::RunLoop loop;
    impression_trakcer_->Init(
        delegate_.get(), base::BindOnce(
                             [](base::RepeatingClosure closure, bool success) {
                               EXPECT_TRUE(success);
                               std::move(closure).Run();
                             },
                             loop.QuitClosure()));
    loop.Run();
  }

  // Verifies the |expected_test_data| matches the internal states.
  void VerifyClientStates(const TestCase& test_case) {
    std::map<SchedulerClientType, const ClientState*> client_states;
    impression_trakcer_->GetClientStates(&client_states);

    ImpressionHistoryTracker::ClientStates expected_client_states;
    test::AddImpressionTestData(test_case.expected, &expected_client_states);

    DCHECK_EQ(expected_client_states.size(), client_states.size());
    for (const auto& expected : expected_client_states) {
      auto output_it = client_states.find(expected.first);
      DCHECK(output_it != client_states.end());
      EXPECT_EQ(*expected.second, *output_it->second)
          << "Unmatch client states: \n"
          << "Expected: \n"
          << test::DebugString(expected.second.get()) << " \n"
          << "Acutual: \n"
          << test::DebugString(output_it->second);
    }
  }

  void SetNow(const char* now_str) { clock_.SetNow(now_str); }

  const SchedulerConfig& config() const { return config_; }
  MockImpressionStore* store() { return store_; }
  MockDelegate* delegate() { return delegate_.get(); }
  ImpressionHistoryTracker* tracker() { return impression_trakcer_.get(); }
  test::FakeClock* clock() { return &clock_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  test::FakeClock clock_;
  SchedulerConfig config_;
  std::unique_ptr<ImpressionHistoryTracker> impression_trakcer_;
  MockImpressionStore* store_;
  std::unique_ptr<MockDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTrackerTest);
};

// New client data should be added to impression tracker.
TEST_F(ImpressionHistoryTrackerTest, NewReigstedClient) {
  TestCase test_case = CreateDefaultTestCase();
  test_case.registered_clients.emplace_back(SchedulerClientType::kTest2);
  test_case.expected.emplace_back(test::ImpressionTestData(
      SchedulerClientType::kTest2, config().initial_daily_shown_per_type, {},
      base::nullopt));

  CreateTracker(test_case);
  EXPECT_CALL(*store(), Add(_, _, _));
  EXPECT_CALL(*delegate(), OnImpressionUpdated()).Times(0);
  InitTrackerWithData(test_case);
  VerifyClientStates(test_case);
}

// Data for deprecated client should be deleted.
TEST_F(ImpressionHistoryTrackerTest, DeprecateClient) {
  TestCase test_case = CreateDefaultTestCase();
  test_case.registered_clients.clear();
  test_case.expected.clear();

  CreateTracker(test_case);
  EXPECT_CALL(*store(), Delete(_, _));
  EXPECT_CALL(*delegate(), OnImpressionUpdated()).Times(0);
  InitTrackerWithData(test_case);
  VerifyClientStates(test_case);
}

// Verifies expired impression will be deleted.
TEST_F(ImpressionHistoryTrackerTest, DeleteExpiredImpression) {
  TestCase test_case = CreateDefaultTestCase();
  auto expired_create_time = clock()->Now() - base::TimeDelta::FromDays(1) -
                             config().impression_expiration;
  auto not_expired_time = clock()->Now() + base::TimeDelta::FromDays(1) -
                          config().impression_expiration;

  Impression expired = CreateImpression(expired_create_time, "guid1");
  Impression not_expired = CreateImpression(not_expired_time, "guid2");

  // The impressions in the input should be sorted by creation time when gets
  // loaded to memory.
  test_case.input.back().impressions = {expired, not_expired, expired};

  // Expired impression created in |expired_create_time| should be deleted.
  // No change expected on the next impression, which is not expired and no user
  // feedback .
  test_case.expected.back().impressions = {not_expired};

  CreateTracker(test_case);
  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _));
  EXPECT_CALL(*delegate(), OnImpressionUpdated());
  tracker()->AnalyzeImpressionHistory();
  VerifyClientStates(test_case);
}

// Verifies the state of new impression added to the tracker.
TEST_F(ImpressionHistoryTrackerTest, AddImpression) {
  TestCase test_case = CreateDefaultTestCase();
  CreateTracker(test_case);
  InitTrackerWithData(test_case);

  // No-op for unregistered client.
  tracker()->AddImpression(SchedulerClientType::kTest2, kGuid1);
  VerifyClientStates(test_case);

  SetNow(kTimeStr);

  EXPECT_CALL(*store(), Update(_, _, _));
  EXPECT_CALL(*delegate(), OnImpressionUpdated());
  tracker()->AddImpression(SchedulerClientType::kTest1, kGuid1);
  test_case.expected.back().impressions.emplace_back(
      Impression(SchedulerClientType::kTest1, kGuid1, clock()->Now()));
  VerifyClientStates(test_case);
}

// If impression has been deleted, click should have no result.
TEST_F(ImpressionHistoryTrackerTest, ClickNoImpression) {
  TestCase test_case = CreateDefaultTestCase();
  CreateTracker(test_case);
  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _)).Times(0);
  EXPECT_CALL(*delegate(), OnImpressionUpdated()).Times(0);
  tracker()->OnClick(SchedulerClientType::kTest1, kGuid1);
  VerifyClientStates(test_case);
}

// Defines the expected state of impression data after certain user action.
struct UserActionTestParam {
  ImpressionResult impression_result = ImpressionResult::kInvalid;
  UserFeedback user_feedback = UserFeedback::kNoFeedback;
  int current_max_daily_show = 0;
  base::Optional<ActionButtonType> button_type;
  bool integrated = false;
  bool has_suppression = false;
  std::map<UserFeedback, ImpressionResult> impression_mapping;
};

class ImpressionHistoryTrackerUserActionTest
    : public ImpressionHistoryTrackerTest,
      public ::testing::WithParamInterface<UserActionTestParam> {
 public:
  ImpressionHistoryTrackerUserActionTest() = default;
  ~ImpressionHistoryTrackerUserActionTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTrackerUserActionTest);
};

const UserActionTestParam kUserActionTestParams[] = {
    // Click.
    {ImpressionResult::kPositive, UserFeedback::kClick, 3, base::nullopt,
     true /*integrated*/, false /*has_suppression*/},
    // Helpful button.
    {ImpressionResult::kPositive, UserFeedback::kHelpful, 3,
     ActionButtonType::kHelpful, true /*integrated*/,
     false /*has_suppression*/},
    // Unhelpful button.
    {ImpressionResult::kNegative, UserFeedback::kNotHelpful, 0,
     ActionButtonType::kUnhelpful, true /*integrated*/,
     true /*has_suppression*/},
    // One dismiss.
    {ImpressionResult::kInvalid, UserFeedback::kDismiss, 2, base::nullopt,
     false /*integrated*/, false /*has_suppression*/},
    // Click with negative impression result from impression mapping.
    {ImpressionResult::kNegative,
     UserFeedback::kClick,
     0,
     base::nullopt,
     true /*integrated*/,
     true /*has_suppression*/,
     {{UserFeedback::kClick,
       ImpressionResult::kNegative}} /*impression_mapping*/}};

// User actions like clicks should update the ClientState data accordingly.
TEST_P(ImpressionHistoryTrackerUserActionTest, UserAction) {
  clock()->SetNow(base::Time::UnixEpoch());
  TestCase test_case = CreateDefaultTestCase();
  Impression impression = CreateImpression(base::Time::Now(), kGuid1);
  DCHECK(!test_case.input.empty());
  impression.impression_mapping = GetParam().impression_mapping;
  test_case.input.front().impressions.emplace_back(impression);

  impression.impression = GetParam().impression_result;
  impression.integrated = GetParam().integrated;
  impression.feedback = GetParam().user_feedback;

  test_case.expected.front().current_max_daily_show =
      GetParam().current_max_daily_show;
  test_case.expected.front().impressions.emplace_back(impression);
  if (GetParam().has_suppression) {
    test_case.expected.front().suppression_info =
        SuppressionInfo(base::Time::UnixEpoch(), config().suppression_duration);
  }

  CreateTracker(test_case);
  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _));
  EXPECT_CALL(*delegate(), OnImpressionUpdated());

  // Trigger user action.
  if (GetParam().user_feedback == UserFeedback::kClick) {
    tracker()->OnClick(SchedulerClientType::kTest1, kGuid1);
  } else if (GetParam().button_type.has_value()) {
    tracker()->OnActionClick(SchedulerClientType::kTest1, kGuid1,
                             GetParam().button_type.value());
  } else if (GetParam().user_feedback == UserFeedback::kDismiss) {
    tracker()->OnDismiss(SchedulerClientType::kTest1, kGuid1);
  }

  VerifyClientStates(test_case);
}

INSTANTIATE_TEST_SUITE_P(,
                         ImpressionHistoryTrackerUserActionTest,
                         testing::ValuesIn(kUserActionTestParams));

}  // namespace

}  // namespace notifications

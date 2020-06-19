#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_scheduling_strategy.h"

#include <memory>
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"

namespace blink {
namespace scheduler {

using FeatureAndParams = ::base::test::ScopedFeatureList::FeatureAndParams;
using ShouldUpdatePolicy =
    ::blink::scheduler::AgentSchedulingStrategy::ShouldUpdatePolicy;

using ::base::FieldTrialParams;
using ::base::sequence_manager::TaskQueue;
using ::base::test::ScopedFeatureList;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::Test;

namespace {

class MockFrameDelegate : public FrameScheduler::Delegate {
 public:
  MockFrameDelegate() {
    ON_CALL(*this, GetAgentClusterId)
        .WillByDefault(ReturnRef(agent_cluster_id_));
  }

  MOCK_METHOD(const base::UnguessableToken&,
              GetAgentClusterId,
              (),
              (const, override));
  MOCK_METHOD(ukm::UkmRecorder*, GetUkmRecorder, ());
  MOCK_METHOD(ukm::SourceId, GetUkmSourceId, ());
  MOCK_METHOD(void, UpdateTaskTime, (base::TimeDelta));
  MOCK_METHOD(void, UpdateActiveSchedulerTrackedFeatures, (uint64_t));

 private:
  base::UnguessableToken agent_cluster_id_ = base::UnguessableToken::Create();
};

class MockFrameScheduler : public FrameSchedulerImpl {
 public:
  explicit MockFrameScheduler(FrameScheduler::FrameType frame_type)
      : FrameSchedulerImpl(nullptr, nullptr, &delegate_, nullptr, frame_type) {}

 private:
  NiceMock<MockFrameDelegate> delegate_;
};

}  // namespace

class PerAgentSchedulingBaseTest : public Test {
 public:
  explicit PerAgentSchedulingBaseTest(
      const FieldTrialParams experiment_params) {
    feature_list_.InitWithFeaturesAndParameters(
        {{kPerAgentSchedulingExperiments, experiment_params}}, {});
    strategy_ = AgentSchedulingStrategy::Create();
    queue_->SetFrameSchedulerForTest(&frame_);
  }

  void SetUp() override {
    ASSERT_EQ(queue_->queue_class(), MainThreadTaskQueue::QueueClass::kTimer);
  }

 protected:
  ScopedFeatureList feature_list_;
  std::unique_ptr<AgentSchedulingStrategy> strategy_;
  MockFrameScheduler frame_{FrameScheduler::FrameType::kMainFrame};
  // Use |kWebScheduling| so that it ends up with |QueueClass::kTimer|
  scoped_refptr<MainThreadTaskQueueForTest> queue_{
      new MainThreadTaskQueueForTest(
          MainThreadTaskQueue::QueueType::kWebScheduling)};
};

class PerAgentDefaultIsNoOpStrategyTest : public PerAgentSchedulingBaseTest {
 public:
  PerAgentDefaultIsNoOpStrategyTest() : PerAgentSchedulingBaseTest({}) {}
};

TEST_F(PerAgentDefaultIsNoOpStrategyTest, DoesntRequestPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnFrameRemoved(frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentDefaultIsNoOpStrategyTest, DoesntModifyPolicyDecisions) {
  EXPECT_FALSE(strategy_->QueueEnabledState(*queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*queue_).has_value());
}

class PerAgentNoOpStrategyTest : public PerAgentSchedulingBaseTest {
 public:
  PerAgentNoOpStrategyTest()
      : PerAgentSchedulingBaseTest({{"study", "no-op"}}) {}
};

TEST_F(PerAgentNoOpStrategyTest, DoesntRequestPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnFrameRemoved(frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentNoOpStrategyTest, DoesntModifyPolicyDecisions) {
  EXPECT_FALSE(strategy_->QueueEnabledState(*queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*queue_).has_value());
}

class PerAgentDisableTimersStrategyTest : public PerAgentSchedulingBaseTest {
 public:
  PerAgentDisableTimersStrategyTest()
      : PerAgentSchedulingBaseTest({{"study", "disable-timers"}}) {}
};

TEST_F(PerAgentDisableTimersStrategyTest, RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(frame_),
            ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnFrameRemoved(frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(frame_),
            ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentDisableTimersStrategyTest, DisablesTimerQueueUntilFMP) {
  ignore_result(strategy_->OnFrameAdded(frame_));
  MockFrameScheduler frame{FrameScheduler::FrameType::kSubframe};
  queue_->SetFrameSchedulerForTest(&frame);

  EXPECT_TRUE(strategy_->QueueEnabledState(*queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*queue_).value());
  EXPECT_FALSE(strategy_->QueuePriority(*queue_).has_value());

  ignore_result(strategy_->OnMainFrameFirstMeaningfulPaint(frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*queue_).has_value());
}

class PerAgentBestEffortPriorityTimersStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentBestEffortPriorityTimersStrategyTest()
      : PerAgentSchedulingBaseTest({{"study", "deprioritize-timers"}}) {}
};

TEST_F(PerAgentBestEffortPriorityTimersStrategyTest, RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(frame_),
            ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnFrameRemoved(frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(frame_),
            ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentBestEffortPriorityTimersStrategyTest,
       LowersTimerQueuePriorityUntilFMP) {
  ignore_result(strategy_->OnFrameAdded(frame_));
  MockFrameScheduler frame{FrameScheduler::FrameType::kSubframe};
  queue_->SetFrameSchedulerForTest(&frame);

  EXPECT_FALSE(strategy_->QueueEnabledState(*queue_).has_value());
  EXPECT_TRUE(strategy_->QueuePriority(*queue_).has_value());
  EXPECT_EQ(strategy_->QueuePriority(*queue_).value(),
            TaskQueue::QueuePriority::kBestEffortPriority);

  ignore_result(strategy_->OnMainFrameFirstMeaningfulPaint(frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*queue_).has_value());
}

}  // namespace scheduler
}  // namespace blink

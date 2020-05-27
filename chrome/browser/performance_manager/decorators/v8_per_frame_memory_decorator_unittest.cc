// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/v8_per_frame_memory_decorator.h"

#include <memory>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class V8PerFrameMemoryDecoratorTest;
using testing::_;

constexpr int kTestProcessID = 0xFAB;
constexpr uint64_t kUnassociatedBytes = 0xABBA;

namespace {

class TestV8PerFrameMemoryDecorator : public V8PerFrameMemoryDecorator {
 public:
  explicit TestV8PerFrameMemoryDecorator(
      base::TimeDelta min_time_between_requests_per_process,
      V8PerFrameMemoryDecoratorTest* fixture)
      : V8PerFrameMemoryDecorator(min_time_between_requests_per_process),
        fixture_(fixture) {}

 private:
  void BindReceiverWithProxyHost(
      mojo::PendingReceiver<
          performance_manager::mojom::V8PerFrameMemoryReporter>
          pending_receiver,
      RenderProcessHostProxy proxy) const override;

  V8PerFrameMemoryDecoratorTest* fixture_ = nullptr;
};

class LenientMockV8PerFrameMemoryReporter
    : public mojom::V8PerFrameMemoryReporter {
 public:
  LenientMockV8PerFrameMemoryReporter() : receiver_(this) {}

  MOCK_METHOD1(GetPerFrameV8MemoryUsageData,
               void(GetPerFrameV8MemoryUsageDataCallback callback));

  void Bind(
      mojo::PendingReceiver<mojom::V8PerFrameMemoryReporter> pending_receiver) {
    return receiver_.Bind(std::move(pending_receiver));
  }

 private:
  mojo::Receiver<mojom::V8PerFrameMemoryReporter> receiver_;
};

using MockV8PerFrameMemoryReporter =
    testing::StrictMock<LenientMockV8PerFrameMemoryReporter>;

void AddPerFrameIsolateMemoryUsage(base::UnguessableToken frame_id,
                                   int64_t world_id,
                                   uint64_t bytes_used,
                                   mojom::PerProcessV8MemoryUsageData* data) {
  if (!base::Contains(data->associated_memory, frame_id)) {
    data->associated_memory[frame_id] = mojom::PerFrameV8MemoryUsageData::New();
  }

  mojom::PerFrameV8MemoryUsageData* per_frame_data =
      data->associated_memory[frame_id].get();
  ASSERT_FALSE(base::Contains(per_frame_data->associated_bytes, world_id));

  auto isolated_world_usage = mojom::V8IsolatedWorldMemoryUsage::New();
  isolated_world_usage->bytes_used = bytes_used;
  per_frame_data->associated_bytes[world_id] = std::move(isolated_world_usage);
}

}  // namespace

class V8PerFrameMemoryDecoratorTest : public GraphTestHarness {
 public:
  static constexpr base::TimeDelta kMinTimeBetweenRequests =
      base::TimeDelta::FromSeconds(30);

  void TearDown() override {
    if (test_decorator_raw_) {
      std::unique_ptr<GraphOwned> graph_owned =
          graph()->TakeFromGraph(test_decorator_raw_);
      EXPECT_EQ(test_decorator_raw_, graph_owned.get());
    }
  }

  TestV8PerFrameMemoryDecorator* CreateDecorator() {
    std::unique_ptr<TestV8PerFrameMemoryDecorator> decorator =
        std::make_unique<TestV8PerFrameMemoryDecorator>(kMinTimeBetweenRequests,
                                                        this);
    test_decorator_raw_ = decorator.get();
    graph()->PassToGraph(std::move(decorator));
    return test_decorator_raw_;
  }

  void ExpectQuery(
      MockV8PerFrameMemoryReporter* mock_reporter,
      base::RepeatingCallback<void(
          MockV8PerFrameMemoryReporter::GetPerFrameV8MemoryUsageDataCallback
              callback)> responder) {
    EXPECT_CALL(*mock_reporter, GetPerFrameV8MemoryUsageData(_))
        .WillOnce(
            [responder](MockV8PerFrameMemoryReporter::
                            GetPerFrameV8MemoryUsageDataCallback callback) {
              responder.Run(std::move(callback));
            });
  }

  void ExpectQueryAndReply(MockV8PerFrameMemoryReporter* mock_reporter,
                           mojom::PerProcessV8MemoryUsageDataPtr data) {
    ExpectQuery(mock_reporter,
                base::BindRepeating(
                    [](mojom::PerProcessV8MemoryUsageDataPtr data,
                       MockV8PerFrameMemoryReporter::
                           GetPerFrameV8MemoryUsageDataCallback callback) {
                      std::move(callback).Run(std::move(data));
                    },
                    base::Passed(&data)));
  }

  void ExpectBindAndRespondToQuery(MockV8PerFrameMemoryReporter* mock_reporter,
                                   mojom::PerProcessV8MemoryUsageDataPtr data) {
    // Wrap the move-only |data| in a callback for the expectation below.
    ExpectQueryAndReply(mock_reporter, std::move(data));

    EXPECT_CALL(*this, BindReceiverWithProxyHost(_, _))
        .WillOnce([mock_reporter](
                      mojo::PendingReceiver<
                          performance_manager::mojom::V8PerFrameMemoryReporter>
                          pending_receiver,
                      RenderProcessHostProxy proxy) {
          DCHECK_EQ(kTestProcessID, proxy.render_process_host_id());
          mock_reporter->Bind(std::move(pending_receiver));
        });
  }

  MOCK_METHOD2(BindReceiverWithProxyHost,
               void(mojo::PendingReceiver<
                        performance_manager::mojom::V8PerFrameMemoryReporter>
                        pending_receiver,
                    RenderProcessHostProxy proxy));

 private:
  TestV8PerFrameMemoryDecorator* test_decorator_raw_ = nullptr;
};

constexpr base::TimeDelta
    V8PerFrameMemoryDecoratorTest::kMinTimeBetweenRequests;

void TestV8PerFrameMemoryDecorator::BindReceiverWithProxyHost(
    mojo::PendingReceiver<performance_manager::mojom::V8PerFrameMemoryReporter>
        pending_receiver,
    RenderProcessHostProxy proxy) const {
  fixture_->BindReceiverWithProxyHost(std::move(pending_receiver), proxy);
}

TEST_F(V8PerFrameMemoryDecoratorTest, InstantiateOnEmptyGraph) {
  auto* decorator = CreateDecorator();

  MockV8PerFrameMemoryReporter mock_reporter;
  auto data = mojom::PerProcessV8MemoryUsageData::New();
  data->unassociated_bytes_used = kUnassociatedBytes;
  ExpectBindAndRespondToQuery(&mock_reporter, std::move(data));

  // Create a process node and validate that it gets a request.
  auto process = CreateNode<ProcessNodeImpl>(
      content::PROCESS_TYPE_RENDERER,
      RenderProcessHostProxy::CreateForTesting(kTestProcessID));

  // Run until idle to make sure the measurement isn't a hard loop.
  task_env().RunUntilIdle();

  EXPECT_EQ(kUnassociatedBytes,
            decorator->GetUnassociatedBytesForTesting(process.get()));
}

TEST_F(V8PerFrameMemoryDecoratorTest, InstantiateOnNonEmptyGraph) {
  // Instantiate the decorator with an existing process node and validate that
  // it gets a request.
  auto process = CreateNode<ProcessNodeImpl>(
      content::PROCESS_TYPE_RENDERER,
      RenderProcessHostProxy::CreateForTesting(kTestProcessID));

  MockV8PerFrameMemoryReporter mock_reporter;
  auto data = mojom::PerProcessV8MemoryUsageData::New();
  data->unassociated_bytes_used = kUnassociatedBytes;
  ExpectBindAndRespondToQuery(&mock_reporter, std::move(data));

  auto* decorator = CreateDecorator();

  // Run until idle to make sure the measurement isn't a hard loop.
  task_env().RunUntilIdle();

  EXPECT_EQ(kUnassociatedBytes,
            decorator->GetUnassociatedBytesForTesting(process.get()));
}

TEST_F(V8PerFrameMemoryDecoratorTest, QueryRateIsLimited) {
  auto process = CreateNode<ProcessNodeImpl>(
      content::PROCESS_TYPE_RENDERER,
      RenderProcessHostProxy::CreateForTesting(kTestProcessID));

  MockV8PerFrameMemoryReporter mock_reporter;
  {
    auto data = mojom::PerProcessV8MemoryUsageData::New();
    // Response to request 1.
    data->unassociated_bytes_used = 1;
    ExpectBindAndRespondToQuery(&mock_reporter, std::move(data));
  }

  auto* decorator = CreateDecorator();

  // Run until idle to make sure the measurement isn't a hard loop.
  task_env().RunUntilIdle();

  EXPECT_EQ(1u, decorator->GetUnassociatedBytesForTesting(process.get()));

  // There shouldn't be an additional request this soon.
  task_env().FastForwardBy(kMinTimeBetweenRequests / 2);
  testing::Mock::VerifyAndClearExpectations(&mock_reporter);

  // Set up another request and capture the callback for later invocation.
  MockV8PerFrameMemoryReporter::GetPerFrameV8MemoryUsageDataCallback callback;
  ExpectQuery(
      &mock_reporter,
      base::BindLambdaForTesting(
          [&callback](
              MockV8PerFrameMemoryReporter::GetPerFrameV8MemoryUsageDataCallback
                  result_callback) { callback = std::move(result_callback); }));

  // Skip forward to when another request should be issued.
  task_env().FastForwardBy(kMinTimeBetweenRequests);
  ASSERT_FALSE(callback.is_null());

  // Skip forward a long while, and validate that no additional requests are
  // issued until the pending request has completed.
  task_env().FastForwardBy(10 * kMinTimeBetweenRequests);
  testing::Mock::VerifyAndClearExpectations(&mock_reporter);

  EXPECT_EQ(1u, decorator->GetUnassociatedBytesForTesting(process.get()));

  // Expect another query once completing the query above.
  {
    auto data = mojom::PerProcessV8MemoryUsageData::New();
    // Response to request 3.
    data->unassociated_bytes_used = 3;
    ExpectQueryAndReply(&mock_reporter, std::move(data));
  }

  // Reply to the request above.
  {
    auto data = mojom::PerProcessV8MemoryUsageData::New();
    // Response to request 2.
    data->unassociated_bytes_used = 2;
    std::move(callback).Run(std::move(data));
  }

  task_env().RunUntilIdle();

  // This should have updated all the way to the third response.
  EXPECT_EQ(3u, decorator->GetUnassociatedBytesForTesting(process.get()));

  // Despite the long delay to respond to request 2, there shouldn't be another
  // request until kMinTimeBetweenRequests has expired.
  task_env().FastForwardBy(kMinTimeBetweenRequests / 2);
  testing::Mock::VerifyAndClearExpectations(&mock_reporter);
}

TEST_F(V8PerFrameMemoryDecoratorTest, MultipleProcessesHaveDistinctSchedules) {
  auto* decorator = CreateDecorator();

  // Create a process node and validate that it gets a request.
  MockV8PerFrameMemoryReporter reporter1;
  {
    auto data = mojom::PerProcessV8MemoryUsageData::New();
    data->unassociated_bytes_used = 1;
    ExpectBindAndRespondToQuery(&reporter1, std::move(data));
  }

  auto process1 = CreateNode<ProcessNodeImpl>(
      content::PROCESS_TYPE_RENDERER,
      RenderProcessHostProxy::CreateForTesting(kTestProcessID));

  task_env().FastForwardBy(kMinTimeBetweenRequests / 4);
  testing::Mock::VerifyAndClearExpectations(&reporter1);

  // Create a second process node and validate that it gets a request.
  MockV8PerFrameMemoryReporter reporter2;
  {
    auto data = mojom::PerProcessV8MemoryUsageData::New();
    data->unassociated_bytes_used = 2;
    ExpectBindAndRespondToQuery(&reporter2, std::move(data));
  }

  auto process2 = CreateNode<ProcessNodeImpl>(
      content::PROCESS_TYPE_RENDERER,
      RenderProcessHostProxy::CreateForTesting(kTestProcessID));

  task_env().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&reporter2);

  EXPECT_EQ(1u, decorator->GetUnassociatedBytesForTesting(process1.get()));
  EXPECT_EQ(2u, decorator->GetUnassociatedBytesForTesting(process2.get()));

  // Capture the request time from each process.
  auto capture_time_lambda =
      [](base::TimeTicks* request_time,
         MockV8PerFrameMemoryReporter::GetPerFrameV8MemoryUsageDataCallback
             callback) {
        *request_time = base::TimeTicks::Now();
        std::move(callback).Run(mojom::PerProcessV8MemoryUsageData::New());
      };

  base::TimeTicks process1_request_time;
  ExpectQuery(&reporter1,
              base::BindRepeating(capture_time_lambda,
                                  base::Unretained(&process1_request_time)));
  base::TimeTicks process2_request_time;
  ExpectQuery(&reporter2,
              base::BindRepeating(capture_time_lambda,
                                  base::Unretained(&process2_request_time)));

  task_env().FastForwardBy(kMinTimeBetweenRequests * 1.25);

  // Check that both processes got polled, and that process2 was polled after
  // process1.
  EXPECT_FALSE(process1_request_time.is_null());
  EXPECT_FALSE(process2_request_time.is_null());
  EXPECT_GT(process2_request_time, process1_request_time);
}

TEST_F(V8PerFrameMemoryDecoratorTest, PerFrameDataIsDistributed) {
  auto* decorator = CreateDecorator();

  MockV8PerFrameMemoryReporter reporter;
  {
    auto data = mojom::PerProcessV8MemoryUsageData::New();
    // Add data for an unknown frame.
    AddPerFrameIsolateMemoryUsage(base::UnguessableToken::Create(), 0, 1024u,
                                  data.get());

    ExpectBindAndRespondToQuery(&reporter, std::move(data));
  }

  auto process = CreateNode<ProcessNodeImpl>(
      content::PROCESS_TYPE_RENDERER,
      RenderProcessHostProxy::CreateForTesting(kTestProcessID));

  task_env().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&reporter);

  // Since the frame was unknown, the usage should have accrued to unassociated.
  EXPECT_EQ(1024u, decorator->GetUnassociatedBytesForTesting(process.get()));

  // Create a couple of frames with specified IDs.
  auto page = CreateNode<PageNodeImpl>();

  base::UnguessableToken frame1_id = base::UnguessableToken::Create();
  auto frame1 = CreateNode<FrameNodeImpl>(process.get(), page.get(), nullptr, 1,
                                          2, frame1_id);

  base::UnguessableToken frame2_id = base::UnguessableToken::Create();
  auto frame2 = CreateNode<FrameNodeImpl>(process.get(), page.get(), nullptr, 3,
                                          4, frame2_id);
  {
    auto data = mojom::PerProcessV8MemoryUsageData::New();
    AddPerFrameIsolateMemoryUsage(frame1_id, 0, 1001u, data.get());
    AddPerFrameIsolateMemoryUsage(frame2_id, 0, 1002u, data.get());
    ExpectQueryAndReply(&reporter, std::move(data));
  }

  task_env().FastForwardBy(kMinTimeBetweenRequests * 1.5);
  testing::Mock::VerifyAndClearExpectations(&reporter);

  EXPECT_EQ(1001u, decorator->GetAssociatedBytesForTesting(frame1.get()));
  EXPECT_EQ(1002u, decorator->GetAssociatedBytesForTesting(frame2.get()));

  // Now verify that data is cleared for any frame that doesn't get an update,
  // plus verify that unknown frame data toes to unassociated bytes.
  {
    auto data = mojom::PerProcessV8MemoryUsageData::New();
    AddPerFrameIsolateMemoryUsage(frame1_id, 0, 1003u, data.get());
    AddPerFrameIsolateMemoryUsage(base::UnguessableToken::Create(), 0, 2233u,
                                  data.get());
    ExpectQueryAndReply(&reporter, std::move(data));
  }
  task_env().FastForwardBy(kMinTimeBetweenRequests);
  testing::Mock::VerifyAndClearExpectations(&reporter);

  EXPECT_EQ(1003u, decorator->GetAssociatedBytesForTesting(frame1.get()));
  EXPECT_FALSE(decorator->HasAssociatedBytesForTesting(frame2.get()));
  EXPECT_EQ(0u, decorator->GetAssociatedBytesForTesting(frame2.get()));
  EXPECT_EQ(2233u, decorator->GetUnassociatedBytesForTesting(process.get()));
}

}  // namespace performance_manager

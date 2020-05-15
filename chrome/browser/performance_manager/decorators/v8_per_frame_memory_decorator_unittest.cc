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
    std::unique_ptr<TestV8PerFrameMemoryDecorator> test_decorator =
        std::make_unique<TestV8PerFrameMemoryDecorator>(kMinTimeBetweenRequests,
                                                        this);
    test_decorator_raw_ = test_decorator.get();
    graph()->PassToGraph(std::move(test_decorator));
    return test_decorator_raw_;
  }

  void ExpectBindAndRespondToQuery(MockV8PerFrameMemoryReporter* mock_reporter,
                                   mojom::PerProcessV8MemoryUsageDataPtr data) {
    // Wrap the move-only |data| in a callback for the expectation below.
    auto wrapper = base::BindRepeating(
        [](mojom::PerProcessV8MemoryUsageDataPtr data,
           MockV8PerFrameMemoryReporter::GetPerFrameV8MemoryUsageDataCallback
               callback) { std::move(callback).Run(std::move(data)); },
        base::Passed(&data));

    EXPECT_CALL(*mock_reporter, GetPerFrameV8MemoryUsageData(_))
        .WillOnce([wrapper](MockV8PerFrameMemoryReporter::
                                GetPerFrameV8MemoryUsageDataCallback callback) {
          wrapper.Run(std::move(callback));
        });

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
  auto* test_decorator_raw = CreateDecorator();

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
            test_decorator_raw->GetUnassociatedBytesForTesting(process.get()));
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

  auto* test_decorator_raw = CreateDecorator();

  // Run until idle to make sure the measurement isn't a hard loop.
  task_env().RunUntilIdle();

  EXPECT_EQ(kUnassociatedBytes,
            test_decorator_raw->GetUnassociatedBytesForTesting(process.get()));
}

// TODO(siggi): More testing.
//   - Test that the request rate doesn't exceed the provided limit.
//   - Test multiple processes.
//   - Test per-frame data.
//   - Test that frame data is cleared when no data is associated with them.
//   - Test that date for unmatched frames (dead) is accrued somewhere.

}  // namespace performance_manager

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/services/pdf_compositor/pdf_compositor_service.h"
#include "components/services/pdf_compositor/public/cpp/pdf_service_mojo_types.h"
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/src/utils/SkMultiPictureDocument.h"

namespace printing {

class PdfCompositorServiceTest : public testing::Test {
 public:
  PdfCompositorServiceTest()
      : connector_(test_connector_factory_.CreateConnector()),
        service_(
            test_connector_factory_.RegisterInstance(mojom::kServiceName)) {
    // We don't want the service instance setting up its own discardable memory
    // allocator, which it normally does. Instead it will use the one provided
    // by our fixture in |SetUp()| below.
    service_.set_skip_initialization_for_testing(true);
  }

  ~PdfCompositorServiceTest() override = default;

  MOCK_METHOD1(CallbackOnCompositeSuccess,
               void(const base::ReadOnlySharedMemoryRegion&));
  MOCK_METHOD1(CallbackOnCompositeStatus, void(mojom::PdfCompositor::Status));

  void OnCompositeToPdfCallback(mojom::PdfCompositor::Status status,
                                base::ReadOnlySharedMemoryRegion region) {
    if (status == mojom::PdfCompositor::Status::kSuccess)
      CallbackOnCompositeSuccess(region);
    else
      CallbackOnCompositeStatus(status);
    run_loop_->Quit();
  }

  MOCK_METHOD0(ConnectionClosed, void());

 protected:
  service_manager::Connector* connector() { return connector_.get(); }

  void SetUp() override {
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);

    ASSERT_FALSE(compositor_);
    connector()->BindInterface(mojom::kServiceName, &compositor_);
    ASSERT_TRUE(compositor_);

    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDown() override {
    compositor_.reset();
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

  base::ReadOnlySharedMemoryRegion CreateMSKP() {
    SkDynamicMemoryWStream stream;
    sk_sp<SkDocument> doc = SkMakeMultiPictureDocument(&stream);
    cc::SkiaPaintCanvas canvas(doc->beginPage(800, 600));
    SkRect rect = SkRect::MakeXYWH(10, 10, 250, 250);
    cc::PaintFlags flags;
    flags.setAntiAlias(false);
    flags.setColor(SK_ColorRED);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas.drawRect(rect, flags);
    doc->endPage();
    doc->close();

    size_t len = stream.bytesWritten();
    base::MappedReadOnlyRegion memory =
        base::ReadOnlySharedMemoryRegion::Create(len);
    CHECK(memory.IsValid());
    stream.copyTo(memory.mapping.memory());
    return std::move(memory.region);
  }

  void CallCompositorWithSuccess(mojom::PdfCompositorPtr ptr) {
    static constexpr uint64_t kFrameGuid = 1234;
    auto handle = CreateMSKP();
    ASSERT_TRUE(handle.IsValid());
    EXPECT_CALL(*this, CallbackOnCompositeSuccess(testing::_)).Times(1);
    ptr->CompositeDocumentToPdf(
        kFrameGuid, std::move(handle), ContentToFrameMap(),
        base::BindOnce(&PdfCompositorServiceTest::OnCompositeToPdfCallback,
                       base::Unretained(this)));
    run_loop_->Run();
  }

  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  mojom::PdfCompositorPtr compositor_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;

 private:
  service_manager::TestConnectorFactory test_connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  PdfCompositorService service_;

  DISALLOW_COPY_AND_ASSIGN(PdfCompositorServiceTest);
};

// Test callback function is called on error conditions in service.
TEST_F(PdfCompositorServiceTest, InvokeCallbackOnContentError) {
  auto serialized_content = base::ReadOnlySharedMemoryRegion::Create(10);

  EXPECT_CALL(*this, CallbackOnCompositeStatus(
                         mojom::PdfCompositor::Status::kContentFormatError))
      .Times(1);
  compositor_->CompositeDocumentToPdf(
      5u, std::move(serialized_content.region), ContentToFrameMap(),
      base::BindOnce(&PdfCompositorServiceTest::OnCompositeToPdfCallback,
                     base::Unretained(this)));
  run_loop_->Run();
}

// Test callback function is called upon success.
TEST_F(PdfCompositorServiceTest, InvokeCallbackOnSuccess) {
  CallCompositorWithSuccess(std::move(compositor_));
}

// Test coexistence of multiple PdfCompositor interface bindings.
TEST_F(PdfCompositorServiceTest, MultipleCompositors) {
  // One service can bind multiple interfaces.
  mojom::PdfCompositorPtr another_compositor;
  ASSERT_FALSE(another_compositor);
  connector()->BindInterface(mojom::kServiceName, &another_compositor);
  ASSERT_TRUE(another_compositor);
  ASSERT_NE(compositor_.get(), another_compositor.get());

  // Terminating one interface won't affect another.
  compositor_.reset();
  CallCompositorWithSuccess(std::move(another_compositor));
}

// Test data structures and content of multiple PdfCompositor interface bindings
// are independent from each other.
TEST_F(PdfCompositorServiceTest, IndependentCompositors) {
  // Create a new connection 2.
  mojom::PdfCompositorPtr compositor2;
  ASSERT_FALSE(compositor2);
  connector()->BindInterface(mojom::kServiceName, &compositor2);
  ASSERT_TRUE(compositor2);

  // In original connection, add frame 4 with content 2 referring
  // to subframe 1.
  compositor_->AddSubframeContent(1u, CreateMSKP(), ContentToFrameMap());

  // Original connection can use this subframe 1.
  EXPECT_CALL(*this, CallbackOnCompositeSuccess(testing::_)).Times(1);
  ContentToFrameMap subframe_content_map;
  subframe_content_map[2u] = 1u;

  compositor_->CompositeDocumentToPdf(
      4u, CreateMSKP(), std::move(subframe_content_map),
      base::BindOnce(&PdfCompositorServiceTest::OnCompositeToPdfCallback,
                     base::Unretained(this)));
  run_loop_->Run();
  testing::Mock::VerifyAndClearExpectations(this);

  // Connection 2 doesn't know about subframe 1.
  subframe_content_map.clear();
  subframe_content_map[2u] = 1u;
  EXPECT_CALL(*this, CallbackOnCompositeSuccess(testing::_)).Times(0);
  compositor2->CompositeDocumentToPdf(
      4u, CreateMSKP(), std::move(subframe_content_map),
      base::BindOnce(&PdfCompositorServiceTest::OnCompositeToPdfCallback,
                     base::Unretained(this)));
  testing::Mock::VerifyAndClearExpectations(this);

  // Add info about subframe 1 to connection 2 so it can use it.
  EXPECT_CALL(*this, CallbackOnCompositeSuccess(testing::_)).Times(1);
  // Add subframe 1's content.
  // Now all content needed for previous request is ready.
  compositor2->AddSubframeContent(1u, CreateMSKP(), ContentToFrameMap());
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
}

}  // namespace printing

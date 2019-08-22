// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/fuchsia/fuchsia_video_decoder.h"

#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/test/scoped_task_environment.h"
#include "components/viz/test/test_context_support.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

class TestBufferCollection {
 public:
  explicit TestBufferCollection(zx::channel collection_token) {
    sysmem_allocator_ = base::fuchsia::ComponentContextForCurrentProcess()
                            ->svc()
                            ->Connect<fuchsia::sysmem::Allocator>();
    sysmem_allocator_.set_error_handler([](zx_status_t status) {
      ZX_LOG(FATAL, status)
          << "The fuchsia.sysmem.Allocator channel was terminated.";
    });

    sysmem_allocator_->BindSharedCollection(
        fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>(
            std::move(collection_token)),
        buffers_collection_.NewRequest());

    fuchsia::sysmem::BufferCollectionConstraints buffer_constraints;
    buffer_constraints.usage.cpu = fuchsia::sysmem::cpuUsageRead;
    zx_status_t status = buffers_collection_->SetConstraints(
        /*has_constraints=*/true, std::move(buffer_constraints));
    ZX_CHECK(status == ZX_OK, status) << "BufferCollection::SetConstraints()";
  }

  ~TestBufferCollection() { buffers_collection_->Close(); }

  size_t GetNumBuffers() {
    if (!buffer_collection_info_) {
      zx_status_t wait_status;
      fuchsia::sysmem::BufferCollectionInfo_2 info;
      zx_status_t status =
          buffers_collection_->WaitForBuffersAllocated(&wait_status, &info);
      ZX_CHECK(status == ZX_OK, status)
          << "BufferCollection::WaitForBuffersAllocated()";
      ZX_CHECK(wait_status == ZX_OK, wait_status)
          << "BufferCollection::WaitForBuffersAllocated()";
      buffer_collection_info_ = std::move(info);
    }
    return buffer_collection_info_->buffer_count;
  }

 private:
  fuchsia::sysmem::AllocatorPtr sysmem_allocator_;
  fuchsia::sysmem::BufferCollectionSyncPtr buffers_collection_;

  base::Optional<fuchsia::sysmem::BufferCollectionInfo_2>
      buffer_collection_info_;

  DISALLOW_COPY_AND_ASSIGN(TestBufferCollection);
};

class TestSharedImageInterface : public gpu::SharedImageInterface {
 public:
  TestSharedImageInterface() = default;
  ~TestSharedImageInterface() override = default;

  gpu::Mailbox CreateSharedImage(viz::ResourceFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 uint32_t usage) override {
    NOTREACHED();
    return gpu::Mailbox();
  }

  gpu::Mailbox CreateSharedImage(
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override {
    NOTREACHED();
    return gpu::Mailbox();
  }

  gpu::Mailbox CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override {
    gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->CloneHandle();
    CHECK_EQ(handle.type, gfx::GpuMemoryBufferType::NATIVE_PIXMAP);

    auto collection_it = sysmem_buffer_collections_.find(
        handle.native_pixmap_handle.buffer_collection_id);
    CHECK(collection_it != sysmem_buffer_collections_.end());
    CHECK_LT(handle.native_pixmap_handle.buffer_index,
             collection_it->second->GetNumBuffers());

    auto result = gpu::Mailbox::Generate();
    mailoxes_.insert(result);
    return result;
  }

  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         const gpu::Mailbox& mailbox) override {
    NOTREACHED();
  }
  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const gpu::Mailbox& mailbox) override {
    NOTREACHED();
  }

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) override {
    CHECK_EQ(mailoxes_.erase(mailbox), 1U);
  }

  SwapChainMailboxes CreateSwapChain(viz::ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     uint32_t usage) override {
    NOTREACHED();
    return SwapChainMailboxes();
  }
  void PresentSwapChain(const gpu::SyncToken& sync_token,
                        const gpu::Mailbox& mailbox) override {
    NOTREACHED();
  }

  void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token) override {
    std::unique_ptr<TestBufferCollection>& collection =
        sysmem_buffer_collections_[id];
    EXPECT_FALSE(collection);
    collection = std::make_unique<TestBufferCollection>(std::move(token));
  }
  void ReleaseSysmemBufferCollection(
      gfx::SysmemBufferCollectionId id) override {
    EXPECT_EQ(sysmem_buffer_collections_.erase(id), 1U);
  }

  gpu::SyncToken GenVerifiedSyncToken() override {
    NOTREACHED();
    return gpu::SyncToken();
  }
  gpu::SyncToken GenUnverifiedSyncToken() override {
    return gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                          gpu::CommandBufferId(33), 1);
  }

  void Flush() override { NOTREACHED(); }

 private:
  base::flat_map<gfx::SysmemBufferCollectionId,
                 std::unique_ptr<TestBufferCollection>>
      sysmem_buffer_collections_;

  base::flat_set<gpu::Mailbox> mailoxes_;
};

}  // namespace

class FuchsiaVideoDecoderTest : public testing::Test {
 public:
  FuchsiaVideoDecoderTest() {
    decoder_ = CreateFuchsiaVideoDecoderForTests(&shared_image_interface_,
                                                 &gpu_context_support_,

                                                 /*enable_sw_decoding=*/true);
  }
  ~FuchsiaVideoDecoderTest() override = default;

  bool Initialize(VideoDecoderConfig config) WARN_UNUSED_RESULT {
    base::RunLoop run_loop;
    bool init_cb_result = false;
    decoder_->Initialize(
        config, true, /*cdm_context=*/nullptr,
        base::BindRepeating(
            [](bool* init_cb_result, base::RunLoop* run_loop, bool result) {
              *init_cb_result = result;
              run_loop->Quit();
            },
            &init_cb_result, &run_loop),
        base::BindRepeating(&FuchsiaVideoDecoderTest::OnVideoFrame,
                            base::Unretained(this)),
        base::NullCallback());

    run_loop.Run();
    return init_cb_result;
  }

  void OnVideoFrame(scoped_refptr<VideoFrame> frame) {
    num_output_frames_++;
    CHECK(frame->HasTextures());
    output_frames_.push_back(std::move(frame));
    while (output_frames_.size() > frames_to_keep_) {
      output_frames_.pop_front();
    }
    if (on_frame_)
      on_frame_.Run();
  }

  DecodeStatus DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
    base::RunLoop run_loop;
    DecodeStatus status;
    decoder_->Decode(buffer,
                     base::BindRepeating(
                         [](DecodeStatus* status, base::RunLoop* run_loop,
                            DecodeStatus result) {
                           *status = result;
                           run_loop->Quit();
                         },
                         &status, &run_loop));

    run_loop.Run();

    return status;
  }

  DecodeStatus ReadAndDecodeFrame(const std::string& name) {
    return DecodeBuffer(ReadTestDataFile(name));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY,
      base::test::TaskEnvironment::MainThreadType::IO};

  TestSharedImageInterface shared_image_interface_;
  viz::TestContextSupport gpu_context_support_;

  std::unique_ptr<VideoDecoder> decoder_;

  std::list<scoped_refptr<VideoFrame>> output_frames_;
  int num_output_frames_ = 0;

  // Number of frames that OnVideoFrame() should keep in |output_frames_|.
  size_t frames_to_keep_ = 2;

  base::RepeatingClosure on_frame_;
};

TEST_F(FuchsiaVideoDecoderTest, CreateAndDestroy) {}

TEST_F(FuchsiaVideoDecoderTest, CreateInitDestroy) {
  EXPECT_TRUE(Initialize(TestVideoConfig::NormalH264()));
}

TEST_F(FuchsiaVideoDecoderTest, DISABLED_VP9) {
  ASSERT_TRUE(Initialize(TestVideoConfig::Normal(kCodecVP9)));

  ASSERT_TRUE(ReadAndDecodeFrame("vp9-I-frame-320x240") == DecodeStatus::OK);
  ASSERT_TRUE(DecodeBuffer(DecoderBuffer::CreateEOSBuffer()) ==
              DecodeStatus::OK);

  EXPECT_EQ(num_output_frames_, 1);
}

TEST_F(FuchsiaVideoDecoderTest, H264) {
  ASSERT_TRUE(Initialize(TestVideoConfig::NormalH264()));

  ASSERT_TRUE(ReadAndDecodeFrame("h264-320x180-frame-0") == DecodeStatus::OK);
  ASSERT_TRUE(ReadAndDecodeFrame("h264-320x180-frame-1") == DecodeStatus::OK);
  ASSERT_TRUE(ReadAndDecodeFrame("h264-320x180-frame-2") == DecodeStatus::OK);
  ASSERT_TRUE(ReadAndDecodeFrame("h264-320x180-frame-3") == DecodeStatus::OK);
  ASSERT_TRUE(DecodeBuffer(DecoderBuffer::CreateEOSBuffer()) ==
              DecodeStatus::OK);

  EXPECT_EQ(num_output_frames_, 4);
}

}  // namespace media

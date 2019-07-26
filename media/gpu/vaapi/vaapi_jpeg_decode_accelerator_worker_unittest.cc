// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_task_environment.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_jpeg_decode_accelerator_worker.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

using testing::_;
using testing::AllOf;
using testing::InSequence;
using testing::IsNull;
using testing::NotNull;
using testing::Property;
using testing::Return;
using testing::StrictMock;

namespace media {
namespace {

constexpr gfx::BufferFormat kFormatForDecodes = gfx::BufferFormat::YVU_420;

class MockNativePixmapDmaBuf : public gfx::NativePixmapDmaBuf {
 public:
  MockNativePixmapDmaBuf(const gfx::Size& size)
      : gfx::NativePixmapDmaBuf(size,
                                kFormatForDecodes,
                                gfx::NativePixmapHandle()) {}

  gfx::NativePixmapHandle ExportHandle() override {
    gfx::NativePixmapHandle handle{};
    DCHECK_EQ(gfx::BufferFormat::YVU_420, GetBufferFormat());
    handle.planes = std::vector<gfx::NativePixmapPlane>(3u);
    return handle;
  }

 protected:
  ~MockNativePixmapDmaBuf() override = default;
};

class MockVaapiJpegDecoder : public VaapiJpegDecoder {
 public:
  MockVaapiJpegDecoder() = default;
  ~MockVaapiJpegDecoder() override = default;

  MOCK_METHOD1(Initialize, bool(const base::RepeatingClosure&));
  MOCK_METHOD1(Decode, VaapiImageDecodeStatus(base::span<const uint8_t>));
  MOCK_CONST_METHOD0(GetScopedVASurface, const ScopedVASurface*());
  MOCK_CONST_METHOD0(GetType, gpu::ImageDecodeAcceleratorType());
  MOCK_CONST_METHOD0(GetSupportedProfile,
                     gpu::ImageDecodeAcceleratorSupportedProfile());
  MOCK_METHOD1(
      ExportAsNativePixmapDmaBuf,
      std::unique_ptr<NativePixmapAndSizeInfo>(VaapiImageDecodeStatus*));
  MOCK_METHOD1(AllocateVASurfaceAndSubmitVABuffers,
               VaapiImageDecodeStatus(base::span<const uint8_t>));
};

}  // namespace

class VaapiJpegDecodeAcceleratorWorkerTest : public testing::Test {
 public:
  VaapiJpegDecodeAcceleratorWorkerTest()
      : worker_(std::make_unique<StrictMock<MockVaapiJpegDecoder>>()) {}

  MockVaapiJpegDecoder* decoder() const {
    return static_cast<MockVaapiJpegDecoder*>(worker_.decoder_.get());
  }

  MOCK_METHOD1(
      OnDecodeCompleted,
      void(std::unique_ptr<gpu::ImageDecodeAcceleratorWorker::DecodeResult>));

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  VaapiJpegDecodeAcceleratorWorker worker_;

  DISALLOW_COPY_AND_ASSIGN(VaapiJpegDecodeAcceleratorWorkerTest);
};

ACTION_P2(ExportAsNativePixmapDmaBufSuccessfully,
          va_surface_resolution,
          visible_size) {
  *arg0 = VaapiImageDecodeStatus::kSuccess;
  auto exported_pixmap = std::make_unique<NativePixmapAndSizeInfo>();
  exported_pixmap->va_surface_resolution = va_surface_resolution;
  exported_pixmap->byte_size = 1u;
  exported_pixmap->pixmap =
      base::MakeRefCounted<MockNativePixmapDmaBuf>(visible_size);
  return exported_pixmap;
}

TEST_F(VaapiJpegDecodeAcceleratorWorkerTest, DecodeSuccess) {
  constexpr gfx::Size kVaSurfaceResolution(128, 256);
  constexpr gfx::Size kVisibleSize(120, 250);
  std::vector<uint8_t> encoded_data = {1u, 2u, 3u};
  {
    InSequence sequence;
    EXPECT_CALL(*decoder(),
                Decode(AllOf(Property(&base::span<const uint8_t>::data,
                                      encoded_data.data()),
                             Property(&base::span<const uint8_t>::size,
                                      encoded_data.size())) /* encoded_data */))
        .WillOnce(Return(VaapiImageDecodeStatus::kSuccess));
    EXPECT_CALL(*decoder(), ExportAsNativePixmapDmaBuf(NotNull() /* status */))
        .WillOnce(ExportAsNativePixmapDmaBufSuccessfully(kVaSurfaceResolution,
                                                         kVisibleSize));
    EXPECT_CALL(*this, OnDecodeCompleted(NotNull()));
  }
  worker_.Decode(
      std::move(encoded_data), kVisibleSize,
      base::BindOnce(&VaapiJpegDecodeAcceleratorWorkerTest::OnDecodeCompleted,
                     base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VaapiJpegDecodeAcceleratorWorkerTest, DecodeFails) {
  constexpr gfx::Size kVisibleSize(120, 250);
  std::vector<uint8_t> encoded_data = {1u, 2u, 3u};
  {
    InSequence sequence;
    EXPECT_CALL(*decoder(),
                Decode(AllOf(Property(&base::span<const uint8_t>::data,
                                      encoded_data.data()),
                             Property(&base::span<const uint8_t>::size,
                                      encoded_data.size())) /* encoded_data */))
        .WillOnce(Return(VaapiImageDecodeStatus::kExecuteDecodeFailed));
    EXPECT_CALL(*this, OnDecodeCompleted(IsNull()));
  }
  worker_.Decode(
      std::move(encoded_data), kVisibleSize,
      base::BindOnce(&VaapiJpegDecodeAcceleratorWorkerTest::OnDecodeCompleted,
                     base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace media
